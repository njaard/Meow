/*  aKode: Player

    Copyright (C) 2004-2005 Allan Sandfeld Jensen <kde@carewolf.com>
    Copyright (C) 2013 Charles Samuels <charles@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#include "audioframe.h"
#include "audiobuffer.h"
#include "decoder.h"
#include "buffered_decoder.h"
#include "mmapfile.h"
#include "localfile.h"
#include "volumefilter.h"

#include "sink.h"
#include "converter.h"
#include "fast_resampler.h"
#include "magic.h"

#include "player.h"

#ifndef NDEBUG
#include <iostream>
#define AKODE_DEBUG(x) {std::cerr << "akode: " << x << "\n";}
#else
#define AKODE_DEBUG(x) { }
#endif

#include <thread>
#include <vector>

namespace aKode
{
    
struct Player::private_data
{
    private_data()
        : buffered_decoder(std::make_shared<BufferedDecoder>())
        , resampler_plugin(&fast_resampler())
    {}

    std::shared_ptr<File> src;

    std::shared_ptr<Decoder> frame_decoder;
    std::shared_ptr<BufferedDecoder> buffered_decoder;
    std::shared_ptr<Resampler> resampler;
    std::shared_ptr<Converter> converter;
    std::shared_ptr<VolumeFilter> volume_filter;
    std::shared_ptr<Sink> sink;
    std::shared_ptr<Player::Manager> manager;
    std::shared_ptr<Player::Monitor> monitor;

    std::shared_ptr<ResamplerPlugin> resampler_plugin;

    std::vector<DecoderPlugin*> registeredDecoders;

    unsigned int sample_rate=0;
    State state=Closed;
    int start_pos=0;

    volatile bool halt=false;
    volatile bool pause=false;
    bool running=false;
    
    std::unique_ptr<std::thread> player_thread;
    sem_t pause_sem;

    void runThread();
};

// The player-thread. It is controlled through the variable halt and pause
void Player::private_data::runThread()
{
    AudioFrame frame;
    AudioFrame re_frame;
    AudioFrame c_frame;
    bool no_error = true;

    while(true)
    {
        if (pause)
        {
            sink->pause();
            sem_wait(&pause_sem);
            sink->resume();
        }
        if (halt) break;

        no_error = buffered_decoder->readFrame(&frame);

        if (!no_error)
        {
            if (buffered_decoder->eof())
                goto eof;
            else if (buffered_decoder->error())
                goto error;
            else
                AKODE_DEBUG("Blip?");
        }
        else
        {
            AudioFrame* out_frame = &frame;
            if (std::shared_ptr<Resampler> r = resampler)
            {
                r->doFrame(out_frame, &re_frame);
                out_frame = &re_frame;
            }

            if (converter)
            {
                converter->doFrame(out_frame, &c_frame);
                out_frame = &c_frame;
            }

            {
                std::shared_ptr<VolumeFilter> volumeFilter = volume_filter;
                if (volumeFilter)
                    volumeFilter->doFrame(out_frame);
            }

            no_error = sink->writeFrame(out_frame);

            if (monitor)
                monitor->writeFrame(out_frame);

            if (!no_error)
            {
                // ### Check type of error
                goto error;
            }
        }
    }

    return;

error:
    {
        buffered_decoder->stop();
        if (manager)
            manager->errorEvent();
    }
    return;
eof:
    {
        buffered_decoder->stop();
        if (manager)
            manager->eofEvent();
    }
}

Player::Player()
{
    d = new private_data;
    sem_init(&d->pause_sem,0,0);
}

Player::~Player()
{
    close();
    sem_destroy(&d->pause_sem);
    delete d;
}

void Player::open(std::shared_ptr<Sink> sink)
{
    if (state() != Closed)
        close();

    assert(state() == Closed);

    d->sink = sink;
    if (!d->sink->open())
    {
        d->sink.reset();
        throw Exception<std::runtime_error>("Could not open sink");
    }
    setState(Open);
}

void Player::close()
{
    if (state() == Closed) return;
    if (state() != Open)
        unload();

    assert(state() == Open);

    d->sink.reset();
    setState(Closed);
}

void Player::load(const FileName& filename)
{
    unload();

    if (state() != Open)
        throw Exception<std::logic_error>("Player::load: called when not open");

    std::shared_ptr<File> src = std::make_shared<MMapFile>(filename);
    // Test if the file _can_ be mmaped
    if (!src->openRO())
    {
#ifndef _WIN32
        std::shared_ptr<File> src = std::make_shared<LocalFile>(filename);
        if (!src->openRO())
        {
            throw Exception<std::runtime_error>("Player::load: failed to open file");
        }
#else
        throw Exception<std::runtime_error>("Player::load: failed to open file");
#endif
    }
    d->src = src;
    // Some of the later code expects it to be closed
    
    d->src->close();

    return load();
}

void Player::load(std::shared_ptr<File> file)
{
    if (state() != Open)
        throw Exception<std::logic_error>("Player::load: called when not open");

    if (!file->openRO())
        throw Exception<std::runtime_error>("Failed to open file");
    // file should be closed before we start playing
    file->close();

    d->src = file;

    load();
}

void Player::load()
{
    try
    {
        for (DecoderPlugin *const decoder : d->registeredDecoders)
        {
            if (decoder->canDecode(d->src.get()))
            {
                d->frame_decoder.reset(decoder->openDecoder(d->src.get()));
                break;
            }
        }

        if (!d->frame_decoder)
            throw Exception<std::runtime_error>("Failed to open Decoder");

        AudioFrame first_frame;

        if (!d->frame_decoder->readFrame(&first_frame))
            throw Exception<std::runtime_error>("Failed to decode first frame");

        int state = d->sink->setAudioConfiguration(&first_frame);
        if (state < 0)
        {
            throw Exception<std::runtime_error>("The sink could not be configured for this format");
        }
        else if (state > 0)
        {
            // Configuration not 100% accurate
            d->sample_rate = d->sink->audioConfiguration()->sample_rate;
            if (d->sample_rate != first_frame.sample_rate)
            {
                AKODE_DEBUG("Resampling to " << d->sample_rate);
                d->resampler.reset(d->resampler_plugin->openResampler());
                if (!d->resampler)
                    throw Exception<std::runtime_error>("The resampler failed to load");
                d->resampler->setSampleRate(d->sample_rate);
            }
            int out_channels = d->sink->audioConfiguration()->channels;
            int in_channels = first_frame.channels;
            if (in_channels != out_channels)
            {
                // ### We don't do mixing yet
                throw Exception<std::runtime_error>("Sample has wrong number of channels");
            }
            int out_width = d->sink->audioConfiguration()->sample_width;
            int in_width = first_frame.sample_width;
            if (in_width != out_width)
            {
                AKODE_DEBUG("Converting to " << out_width << "bits");
                if (!d->converter)
                    d->converter.reset(new Converter(out_width));
                else
                    d->converter->setSampleWidth(out_width);
            }
        }
        else
        {
            d->resampler.reset();
            d->converter.reset();
        }

        // connect the streams to play
        d->buffered_decoder->setBlockingRead(true);
        d->buffered_decoder->openDecoder(d->frame_decoder.get());
        d->buffered_decoder->buffer()->put(&first_frame);

        setState(Loaded);
    }
    catch (...)
    {
        d->resampler.reset();
        d->converter.reset();
        d->frame_decoder.reset();
        d->src.reset();
        
        throw;
    }
}

void Player::unload()
{
    if (state() == Open) return;
    stop();
    if (state() != Loaded)
        throw Exception<std::logic_error>("Player::unload called when not loaded");

    d->buffered_decoder->closeDecoder();

    d->frame_decoder.reset();
    d->src.reset();

    d->resampler.reset();
    d->converter.reset();
    
    setState(Open);
}

void Player::play()
{
    if (state() != Loaded && state() != Paused)
        throw Exception<std::logic_error>("Player::play called when not loaded");

    if (state() == Paused)
    {
        resume();
        return;
    }
    d->frame_decoder->seek(0);

    // Start buffering
    d->buffered_decoder->start();

    d->halt = d->pause = false;

    try
    {
        d->player_thread.reset(new std::thread(std::bind(&private_data::runThread, d)));
        d->running = true;
        setState(Playing);
    }
    catch (...)
    {
        setState(Loaded);
    }
}

void Player::stop()
{
    if (state() == Open || state() == Loaded) return;
    if (state() != Playing && state() != Paused)
        throw Exception<std::logic_error>("Player::stop called when not playing or paused");

    // Needs to set halt first to avoid the paused thread playing a soundbite
    d->halt = true;
    if (state() == Paused) resume();

    assert(state() == Playing);

    d->buffered_decoder->stop();

    if (d->running)
    {
        d->player_thread->join();
        d->player_thread.reset();
        d->running = false;
    }

    setState(Loaded);
}

void Player::pause()
{
    if (state() == Paused) return;
    if (state() != Playing)
        throw Exception<std::logic_error>("Player::pause called when not playing");

    //d->buffer->pause();
    d->pause = true;
    setState(Paused);
}

void Player::resume()
{
    if (state() == Playing) return;
    if (state() != Paused)
        throw Exception<std::logic_error>("Player::resume called when not paused");

    d->pause = false;
    sem_post(&d->pause_sem);

    setState(Playing);
}


void Player::setVolume(float f)
{
    if (f < 0.0 || f > 1.0) return;

    std::shared_ptr<VolumeFilter> vf = d->volume_filter;
    
    if (f != 1.0 && !vf)
    {
        vf = std::make_shared<VolumeFilter>();
        vf->setVolume(f);
        d->volume_filter = vf;
    }
    else if (f != 1.0)
    {
        vf->setVolume(f);
    }
    else if (vf)
    {
        vf.reset();
    }
}

float Player::volume() const
{
    if (std::shared_ptr<VolumeFilter> vf = d->volume_filter)
        return vf->volume();
    else
        return 1.0;
}

std::shared_ptr<File> Player::file() const 
{
    return d->src;
}

std::shared_ptr<Sink> Player::sink() const
{
    return d->sink;
}

std::shared_ptr<Decoder> Player::decoder() const
{
    return d->buffered_decoder;
}

std::shared_ptr<Resampler> Player::resampler() const
{
    return d->resampler;
}

Player::State Player::state() const
{
    return d->state;
}

void Player::registerDecoderPlugin(DecoderPlugin *decoder)
{
    d->registeredDecoders.push_back(decoder);
}

void Player::setResamplerPlugin(std::shared_ptr<ResamplerPlugin> resampler)
{
    d->resampler_plugin = resampler;
}

void Player::setManager(std::shared_ptr<Manager> manager)
{
    d->manager = manager;
}

void Player::setMonitor(std::shared_ptr<Monitor> monitor)
{
    d->monitor = monitor;
}

void Player::setState(Player::State state)
{
    d->state = state;
    if (d->manager)
        d->manager->stateChangeEvent(state);
}


} // namespace
