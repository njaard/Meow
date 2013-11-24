/*  aKode: Ogg Vorbis-Decoder

    Copyright (C) 2004 Allan Sandfeld Jensen <kde@carewolf.com>
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

#include <vorbis/vorbisfile.h>

#include <akode/file.h>
#include <akode/audioframe.h>
#include <akode/decoder.h>
#include "vorbis_decoder.h"

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <limits>

using namespace aKode;

namespace
{

class VorbisDecoder : public Decoder
{
public:
    VorbisDecoder(File* src);
    virtual ~VorbisDecoder();

    virtual bool openFile();

    virtual bool readFrame(AudioFrame*);
    virtual long length();
    virtual long position();
    virtual bool seek(long);
    virtual bool seekable();
    virtual bool eof();
    virtual bool error();

    virtual const AudioConfiguration* audioConfiguration();

    struct private_data;
private:
    private_data *m_data;
};

    
static size_t _read(void *ptr, size_t size, size_t nmemb, void *datasource) {
    File *src = (File*)datasource;
    return src->read((char*)ptr, size*nmemb);
}

static int _seek(void *datasource, ogg_int64_t offset, int whence) {
    File *src = (File*)datasource;
    if (src->seek(offset, whence))
        return 0;
    else
        return -1;
}

static int _close(void *datasource) {
    File *src = (File*)datasource;
    src->close();
    return 0;
}

static long _tell(void *datasource) {
    File *src = (File*)datasource;
    return src->position();
}

static ov_callbacks _callbacks = {_read, _seek, _close, _tell};



struct VorbisDecoder::private_data
{
    private_data()
        : bitstream(0), eof(false), error(false),
        initialized(false), retries(0), big_endian(0),
        trackGain(false), albumGain(false) {}
    OggVorbis_File *vf;
    vorbis_comment *vc;
    vorbis_info *vi;

    File *src;
    AudioConfiguration config;

    int bitstream;
    bool eof, error;
    char buffer[8192];
    bool initialized;
    int retries;

    int big_endian;
    
    bool trackGain, albumGain;
    uint64_t trackGainValue, albumGainValue;
};

VorbisDecoder::VorbisDecoder(File *src) {
    m_data = new private_data;
    m_data->vf = new OggVorbis_File;

    m_data->src = src;
    m_data->src->openRO();
    m_data->src->fadvise();

    unsigned short endian_test = 1;
    m_data->big_endian = 1 - (*((char *)(&endian_test)));
}

VorbisDecoder::~VorbisDecoder() {
    if (m_data->initialized)
        ov_clear(m_data->vf);
    delete m_data->vf;
    delete m_data;
}

static void setAudioConfiguration(AudioConfiguration *config, vorbis_info *vi)
{
    config->channels = vi->channels;
    config->sample_rate = vi->rate;
    config->sample_width = 16;

    if (config->channels <= 2) {
        config->channel_config = MonoStereo;
        config->surround_config = 0;
    } else
    if (config->channels <= 6) {
        config->channel_config = Surround;
        SurroundConfiguration surround_config;
        switch (config->channels) {
            case 3:
                surround_config.front_channels = 3;
                break;
            case 4:
                surround_config.front_channels = 2;
                surround_config.rear_channels = 2;
                break;
            case 5:
                surround_config.front_channels = 3;
                surround_config.rear_channels = 2;
                break;
            case 6:
                surround_config.front_channels = 3;
                surround_config.rear_channels = 2;
                surround_config.LFE_channel = 1;
                break;
        }
        config->surround_config = surround_config;
    }
    else {
        config->channel_config = MultiChannel;
        config->surround_config = 0;
    }
}

bool VorbisDecoder::openFile() {
    int status;

    status = ov_open_callbacks(m_data->src, m_data->vf, 0, 0, _callbacks);
    if (status != 0) goto fault;

    m_data->vi = ov_info(m_data->vf, -1);
    m_data->vc = ov_comment(m_data->vf, -1);
    setAudioConfiguration(&m_data->config, m_data->vi);

    for ( int i=0; i < m_data->vc->comments; i++ )
    {
        std::string comment = m_data->vc->user_comments[i];
        const size_t eq = comment.find('=');
        if (eq == std::string::npos) continue;
        std::string name = comment.substr(0, eq);
        std::string value = comment.substr(eq+1);
    
        if (name == "REPLAYGAIN_TRACK_GAIN" || name=="RG_RADIO")
        {
            m_data->trackGainValue = std::pow(10, std::atof(value.c_str())/20.0) * std::numeric_limits<int16_t>::max();
            m_data->trackGain = true;
            // std::cerr << "track gain " << m_data->trackGainValue << std::endl;
        }
        else if (name == "REPLAYGAIN_ALBUM_GAIN" || name=="RG_AUDIOPHILE")
        {
            m_data->albumGainValue = std::pow(10, std::atof(value.c_str())/20.0) * std::numeric_limits<int16_t>::max();
            m_data->albumGain = true;
            // std::cerr << "album gain " << m_data->albumGainValue << std::endl;
        }
    }
    m_data->initialized = true;
    m_data->error = false;
    m_data->retries = 0;
    return true;
fault:
    m_data->initialized = false;
    m_data->error = true;
    return false;
}

// translation from vorbis channel-layout to akodelib channel-layout
static int vorbis_channel[7][6] = {
    {-1, -1, -1, -1, -1, -1},
    {0, -1, -1, -1, -1, -1},
    {0, 1, -1, -1, -1, -1},
    {0, 2, 1, -1, -1, -1},
    {0, 1, 2, 3, -1, -1},
    {0, 2, 1, 3, 4, -1},
    {0, 2, 1, 3, 4, 5}
};

bool VorbisDecoder::readFrame(AudioFrame* frame)
{
    if (!m_data->initialized) {
        if (!openFile()) return false;
    }

    int old_bitstream = m_data->bitstream;
    long v = ov_read(m_data->vf, (char*)m_data->buffer, 8192, m_data->big_endian, 2, 1, &m_data->bitstream);

    if (v == 0 || v == OV_EOF ) {
        // vorbisfile sometimes return 0 even though EOF is not yet reached
        if (m_data->src->eof() || m_data->src->error() || ++m_data->retries >= 16)
            m_data->eof = true;
//        std::cerr << "akode-vorbis: EOF\n";
    }
    else
    if (v == OV_HOLE) {
        if (++m_data->retries >= 16) m_data->error = true;
//         std::cerr << "akode-vorbis: Hole\n";
    }
    else
    if (v < 0) {
        m_data->error = true;
//         std::cerr << "akode-vorbis: Error\n";
    }

    if (v <= 0) return false;
    m_data->retries = 0;

    if (old_bitstream != m_data->bitstream) { // changing streams, update info
        m_data->vi = ov_info(m_data->vf, -1);
        //m_data->vc = ov_comment(m_data->vf, -1);
        setAudioConfiguration(&m_data->config, m_data->vi);
    }

    int channels = m_data->config.channels;
    long length = v/(channels*2);
    frame->reserveSpace(&m_data->config, length);

    // Demux into frame
    int16_t* buffer = (int16_t*)m_data->buffer;
    int16_t** data = (int16_t**)frame->data;
    if (channels <= 6) {
        int *trans = vorbis_channel[channels];
        for(int i=0; i<length; i++)
            for(int j=0; j<channels; j++)
                data [trans[j]] [i] = buffer[i*channels+j];
    }
    else
        for(int i=0; i<length; i++)
            for(int j=0; j<channels; j++)
                data[j][i] = buffer[i*channels+j];

    if (m_data->trackGain)
    {
        for(int i=0; i<length; i++)
            for(int j=0; j<channels; j++)
                data[j][i] = int32_t(data[j][i]) * m_data->trackGainValue / std::numeric_limits<int16_t>::max();
    }
    frame->pos = position();
    return true;
}

long VorbisDecoder::length() {
    if (!m_data->initialized) return -1;
    // -1 return total length of all bitstreams.
    // Should we take them one at a time instead?
    double ogglen = ov_time_total(m_data->vf,-1);
    return (long)(ogglen*1000.0);
}

long VorbisDecoder::position() {
    if (!m_data->initialized) return -1;
    double oggpos = ov_time_tell(m_data->vf);
    return (long)(oggpos*1000.0);
}

bool VorbisDecoder::eof() {
    return m_data->eof;
}

bool VorbisDecoder::error() {
    return m_data->error;
}

bool VorbisDecoder::seekable() {
    return m_data->src->seekable();
}

bool VorbisDecoder::seek(long pos) {
    if (!m_data->initialized) return false;
    int r = ov_time_seek(m_data->vf, pos/1000.0);
    return r == 0;
}

const AudioConfiguration* VorbisDecoder::audioConfiguration() {
    if (!m_data->initialized) return 0;
    return &m_data->config;
}


class VorbisDecoderPlugin : public DecoderPlugin
{
public:
    VorbisDecoderPlugin() : DecoderPlugin("vorbis") { }
    virtual bool canDecode(File* src)
    {
        OggVorbis_File vf;
        src->openRO();
        int r = ov_test_callbacks(src, &vf, 0, 0, _callbacks);
        ov_clear(&vf);
        src->close();
        return r==0;
    }
    virtual VorbisDecoder* openDecoder(File* src)
    {
        return new VorbisDecoder(src);
    }
} plugin;

} // namespace

namespace aKode
{
DecoderPlugin& vorbis_decoder()
{
    return plugin;
}

} // namespace


