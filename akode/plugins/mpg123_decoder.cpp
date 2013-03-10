/*  aKode: MP3-Decoder (mpg123-decoder)

    Copyright (C) 2013 Charles Samuels <charles@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston,
    MA  02110-1301, USA.
*/

#include <akode/akodelib.h>

#include <mpg123.h>

#include <string.h>

#include <iostream>

#include <akode/file.h>
#include <akode/audioframe.h>
#include "mpg123_decoder.h"

#include <stdexcept>

using namespace aKode;

namespace
{

static bool wasInitialized=false;

static ssize_t fileRead(void *_f, void *data, size_t count)
{
    File *const f = reinterpret_cast<File*>(_f);
    return f->read(reinterpret_cast<char*>(data), count);
}

static off_t fileSeek(void *_f, off_t pos, int whence)
{
    File *const f = reinterpret_cast<File*>(_f);
    
    f->seek(pos, whence);
    return f->position();
}

static void cleanup(void*)
{
    // do nothing
}

    
class MPG123Decoder : public Decoder
{
    File *const src;
    AudioConfiguration config;
    
    bool mEof, mError;

    mpg123_handle *mpg123;
    
public:
    MPG123Decoder(File* src);
    virtual ~MPG123Decoder();

    virtual bool readFrame(AudioFrame*);
    virtual long length()
    {
        return int64_t(mpg123_length(mpg123))*1000/config.sample_rate;
    }
    virtual long position()
    {
        return int64_t(mpg123_tell(mpg123))*1000/config.sample_rate;
    }
    virtual bool seek(long ms)
    {
        mpg123_seek(mpg123, int64_t(ms)*config.sample_rate/1000, SEEK_SET);
        return true;
    }
    virtual bool seekable()
    {
        return src->seekable();
    }
    virtual bool eof()
    {
        return mEof;
    }
    virtual bool error()
    {
        return mError;
    }

    virtual const AudioConfiguration* audioConfiguration();
};


static void maybeThrowError(int e)
{
    if (e != MPG123_OK)
        throw std::runtime_error("mpg123 error: " + std::string(mpg123_plain_strerror(e)));
}


MPG123Decoder::MPG123Decoder(File *src)
    : src(src)
{
    int err;
    if (!wasInitialized)
    {
        err = mpg123_init();
        maybeThrowError(err);
        wasInitialized=true;
    }
    
    mEof = false;
    mError = false;

    if (!src->openRO())
    {
        mError = true;
    }
    src->fadvise();
    
    mpg123 = mpg123_new(0, &err);
    maybeThrowError(err);
    
    err = mpg123_param(mpg123, MPG123_FLAGS, MPG123_FUZZY | MPG123_SEEKBUFFER | MPG123_GAPLESS, 0);
    maybeThrowError(err);
    err = mpg123_param(mpg123, MPG123_VERBOSE, 100, 0);
    maybeThrowError(err);
    
    mpg123_replace_reader_handle(mpg123, fileRead, fileSeek, cleanup);
    mpg123_open_handle(mpg123, src);
    maybeThrowError(err);
    
    
    long flags;
    err = mpg123_getparam(mpg123, MPG123_FLAGS, &flags, 0);
    if (!(flags & MPG123_GAPLESS))
    {
        std::cerr << "No gapless playback" << std::endl;
    }
}

MPG123Decoder::~MPG123Decoder()
{
    mpg123_delete(mpg123);
}


// originaly from minimad.c
template<int bits>
static inline int32_t scale(int16_t sample)
{
    return sample;
}



bool MPG123Decoder::readFrame(AudioFrame* frame)
{
    while (1)
    {
        unsigned char outblock[1024*12];
        
        size_t done;
        int err = mpg123_read(mpg123, outblock, sizeof(outblock), &done);
        
        if (err == MPG123_NEW_FORMAT)
        {
            long rate;
            int channels;
            int encoding;
            mpg123_getformat(mpg123, &rate, &channels, &encoding);
            
            mpg123_format_none(mpg123);
            mpg123_format(mpg123, rate, MPG123_STEREO, MPG123_ENC_SIGNED_16);

            config.channels = channels;
            config.channel_config = MonoStereo;
            config.sample_rate = rate;
            config.sample_width = 16;
            // doRead=false;
            continue;
        }
        else if (err == MPG123_DONE)
        {
            mEof = true;
            frame->length=0;
            return false;
        }
        else if (err == MPG123_OK)
        {
            const int numSamples = done/config.channels/2;
            frame->reserveSpace(&config, numSamples);
            
            int16_t** data = (int16_t**)frame->data;
            int16_t* samples = (int16_t*)outblock;
            for(int i=0; i<numSamples; i++)
                for(int j=0; j<config.channels; j++)
                    data[j][i] = scale<16>(samples[i*config.channels+j]);
            frame->pos = position();
        }
        else
        {
            maybeThrowError(err);
        }
        return true;
    }
}


const AudioConfiguration* MPG123Decoder::audioConfiguration()
{
    return &config;
}


class MPG123DecoderPlugin : public DecoderPlugin
{
public:
    MPG123DecoderPlugin() : DecoderPlugin("mpeg") { }
    virtual bool canDecode(File* src)
    {
        char header[6];
        unsigned char *buf = (unsigned char*)header; // C-stuff
        bool res = false;
        src->openRO();
        if(src->read(header, 4))
        {
            // skip id3v2 headers
            if (memcmp(header, "ID3", 3) == 0)
            {
                res = true;
            }
            else if (buf[0] == 0xff && (buf[1] & 14)) // frame synchronizer
                if((buf[1] & 0x18) != 0x08) // support MPEG 1, 2 and 2�
                    if((buf[1] & 0x06) != 0x00) // Layer I, II and III
                        res = true;
        }
        src->close();
        return res;
    }
    virtual MPG123Decoder* openDecoder(File* src)
    {
        return new MPG123Decoder(src);
    }
} plugin;

} // namespace

namespace aKode
{
DecoderPlugin& mpg123_decoder()
{
    return plugin;
}
}
