#ifdef AKODE_WITH_OPUS

/*  aKode: Opus-Decoder

    Copyright (C) 2013 Charles Samuels <charles@kde.org>
    Copyright (C) 2004 Allan Sandfeld Jensen <kde@carewolf.com>

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

#include <opus.h>
#include <opusfile.h>

#include <akode/file.h>
#include <akode/audioframe.h>
#include <akode/decoder.h>
#include "opus_decoder.h"

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <limits>

using namespace aKode;

namespace
{

class OpusDecoder : public Decoder
{
public:
    OpusDecoder(File* src);
    virtual ~OpusDecoder();

    virtual bool openFile();

    virtual bool readFrame(AudioFrame*);
    virtual long length();
    virtual long position();
    virtual bool seek(long);
    virtual bool seekable();
    virtual bool eof();
    virtual bool error();

    virtual const AudioConfiguration* audioConfiguration();

private:
    OggOpusFile *vf;

    File *src;
    AudioConfiguration config;

    bool mEof, mError;
    static const unsigned pcmSize=11520*2;
    opus_int16 pcm[pcmSize];
    bool mInitialized;
    int mRetries;
};

    
static int _read(void *datasource, unsigned char *data, int nbytes)
{
    File *src = (File*)datasource;
    return src->read((char*)data, nbytes);
}

static int _seek(void *datasource, opus_int64 offset, int whence)
{
    File *src = (File*)datasource;
    if (src->seek(offset, whence))
        return 0;
    else
        return -1;
}

static int _close(void *datasource)
{
    File *src = (File*)datasource;
    src->close();
    return 0;
}

static opus_int64 _tell(void *datasource)
{
    File *src = (File*)datasource;
    return src->position();
}

static const OpusFileCallbacks _callbacks = {_read, _seek, _tell, _close};


OpusDecoder::OpusDecoder(File *src)
{
    mInitialized=false;
    
    this->src = src;
    src->openRO();
    src->fadvise();
    
}

OpusDecoder::~OpusDecoder()
{
    if (mInitialized)
        op_free(vf);
}

bool OpusDecoder::openFile()
{
    int status;

    vf = op_open_callbacks(src, &_callbacks, 0, 0, &status);
    if (status != 0) goto fault;

    // TODO gain: opus_tags_get_track_gain
    
    config.channels = 2;
    config.sample_rate = 48000;
    config.sample_width = 16;
    config.channel_config = MonoStereo;
    config.surround_config = 0;

    mInitialized = true;
    mError = false;
    mRetries = 0;
    return true;
fault:
    mInitialized = false;
    mError = true;
    return false;
}

bool OpusDecoder::readFrame(AudioFrame* frame)
{
    if (!mInitialized)
    {
        if (!openFile()) return false;
    }

    int v = op_read_stereo(vf, pcm, pcmSize);

    if (v == 0)
    {
        // vorbisfile sometimes return 0 even though EOF is not yet reached
        if (src->eof() || src->error() || ++mRetries >= 16)
            mEof = true;
//        std::cerr << "akode-vorbis: EOF\n";
    }
    else if (v == OP_HOLE)
    {
        if (++mRetries >= 16) mError = true;
//         std::cerr << "akode-vorbis: Hole\n";
    }
    else if (v < 0)
    {
        mError = true;
//         std::cerr << "akode-vorbis: Error\n";
    }

    if (v <= 0) return false;
    mRetries = 0;

    const int channels = 2;
    const int length = v;
    frame->reserveSpace(&config, length);

    // Demux into frame
    const opus_int16* buffer = pcm;
    int16_t** data = (int16_t**)frame->data;
    for(int i=0; i<length; i++)
        for(int j=0; j<channels; j++)
            data[j][i] = buffer[i*channels+j];

    frame->pos = position();
    return true;
}

long OpusDecoder::length()
{
    if (!mInitialized) return -1;
    // -1 return total length of all bitstreams.
    // Should we take them one at a time instead?
    opus_int64 len = op_pcm_total(vf,-1);
    return len/48000*1000;
}

long OpusDecoder::position()
{
    if (!mInitialized) return -1;
    opus_int64 pos = op_pcm_tell(vf);
    return pos/48000*1000;
}

bool OpusDecoder::eof()
{
    return mEof;
}

bool OpusDecoder::error()
{
    return mError;
}

bool OpusDecoder::seekable()
{
    return src->seekable();
}

bool OpusDecoder::seek(long pos)
{
    if (!mInitialized) return false;
    int r = op_pcm_seek(vf, pos*(opus_int64)48000/1000);
    return r == 0;
}

const AudioConfiguration* OpusDecoder::audioConfiguration()
{
    if (!mInitialized) return 0;
    return &config;
}


class OpusDecoderPlugin: public DecoderPlugin
{
public:
    OpusDecoderPlugin() : DecoderPlugin("opus") { }
    virtual bool canDecode(File* src)
    {
        src->openRO();
        int error;
        OggOpusFile *const vf = op_test_callbacks(src, &_callbacks, 0, 0, &error);
        op_free(vf);
        src->close();
        return !!vf;
    }
    virtual OpusDecoder* openDecoder(File* src)
    {
        return new OpusDecoder(src);
    }
} plugin;

} // namespace

namespace aKode
{
DecoderPlugin& opus_decoder()
{
    return plugin;
}

} // namespace


#endif
