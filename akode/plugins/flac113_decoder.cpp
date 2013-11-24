/*  aKode: FLAC-Decoder (using libFLAC 1.1.3 API)

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

#ifdef HAVE_LIBFLAC113

#include <string.h>
#include <iostream>

#include <FLAC/format.h>
#include <FLAC/stream_decoder.h>

#include <akode/file.h> 
#include <akode/audioframe.h>
#include <akode/decoder.h>
#include "flac113_decoder.h"

#include <cmath>
#include <limits>
#include <cstdlib>


using namespace aKode;

namespace
{

class FLACDecoder : public Decoder
{
public:
    FLACDecoder(File* src);
    virtual ~FLACDecoder();
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

static bool checkFLAC(File *src)
{
    char header[6];
    bool res = false;
    src->seek(0);
    if(src->read(header, 4) == 4) {
        // skip id3v2 headers
        if (memcmp(header, "ID3", 3) == 0) {
            if (src->read(header, 6) != 6) goto end;
            int size = 0;
            if (header[1] & 0x10) size += 10;
            size += header[5];
            size += header[4] << 7;
            size += header[3] << 14;
            size += header[2] << 21;
            src->seek(10+size);
            if (src->read(header, 4) != 4) goto end;
        }
        if (memcmp(header, "fLaC",4) == 0 ) res = true;
    }
end:
    return res;
}

struct FLACDecoder::private_data {
    private_data() : decoder(0), valid(false), out(0), source(0), eof(false), error(false)
    , trackGain(false), albumGain(false)
    {}

    FLAC__StreamDecoder *decoder;
    const FLAC__StreamMetadata_StreamInfo* si;
    const FLAC__StreamMetadata_VorbisComment* vc;

    bool valid;
    AudioFrame *out;
    File *source;
    AudioConfiguration config;

    uint32_t max_block_size;
    uint64_t position, length;

    bool eof, error;
    bool trackGain, albumGain;
    uint64_t trackGainValue, albumGainValue;
};

static FLAC__StreamDecoderReadStatus flac_read_callback(
        const FLAC__StreamDecoder *,
        FLAC__byte buffer[],
        size_t *bytes,
        void *client_data)
{
    FLACDecoder::private_data *data = (FLACDecoder::private_data*)client_data;

    long res = data->source->read((char*)buffer, *bytes);
    if (res<=0) {
        if (data->source->eof()) {
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        }
        else
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }
    else {
        *bytes = res;
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
}

static FLAC__StreamDecoderSeekStatus flac_seek_callback(
        const FLAC__StreamDecoder *,
        FLAC__uint64 absolute_byte_offset,
        void *client_data)
{
    FLACDecoder::private_data *data = (FLACDecoder::private_data*)client_data;
    if(data->source->seek(absolute_byte_offset))
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    else
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

static FLAC__StreamDecoderTellStatus flac_tell_callback(
        const FLAC__StreamDecoder *,
        FLAC__uint64 *absolute_byte_offset,
        void *client_data)
{
    FLACDecoder::private_data *data = (FLACDecoder::private_data*)client_data;

    long res = data->source->position();
    if (res<0)
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    else {
        *absolute_byte_offset = res;
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
    }
}

static FLAC__StreamDecoderLengthStatus flac_length_callback(
        const FLAC__StreamDecoder *,
        FLAC__uint64 *stream_length,
        void *client_data)
{
    FLACDecoder::private_data *data = (FLACDecoder::private_data*)client_data;

    long res = data->source->length();
    if (res<0)
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    else {
        *stream_length = res;
        return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
    }
}

static FLAC__bool eof_callback(
        const FLAC__StreamDecoder *,
        void *client_data)
{
    FLACDecoder::private_data *data = (FLACDecoder::private_data*)client_data;

    return data->source->eof();
}

static FLAC__StreamDecoderWriteStatus write_callback(
        const FLAC__StreamDecoder *,
        const FLAC__Frame *frame,
        const FLAC__int32 * const buffer[],
        void* client_data)
{
    FLACDecoder::private_data *m_data = (FLACDecoder::private_data*)client_data;

    if (!m_data->out)  // Handle spurious callbacks (happens during seeks)
        m_data->out = new AudioFrame;

    const long frameSize = frame->header.blocksize;
    const char bits = frame->header.bits_per_sample;
    const char channels = frame->header.channels;

    AudioFrame* const outFrame = m_data->out;

    outFrame->reserveSpace(channels, frameSize, bits);
    outFrame->sample_rate = frame->header.sample_rate;

    if (channels == 1 || channels == 2)
        outFrame->channel_config = aKode::MonoStereo;
    else if (channels > 2 && channels < 8)
        outFrame->channel_config = aKode::Surround;
    else
        outFrame->channel_config = aKode::MultiChannel;

    for(int i = 0; i<channels; i++)
    {
        if (outFrame->data[i] == 0) break;
        if (bits<=8)
        {
            int8_t** data = (int8_t**)outFrame->data;
            for(long j=0; j<frameSize; j++)
                data[i][j] = buffer[i][j];
            if (m_data->trackGain)
            {
                for(long j=0; j<frameSize; j++)
                    data[i][j] = int32_t(data[i][j]) * m_data->trackGainValue / std::numeric_limits<int8_t>::max();
            }
        }
        else if (bits<=16)
        {
            int16_t** data = (int16_t**)outFrame->data;
            for(long j=0; j<frameSize; j++)
                data[i][j] = buffer[i][j];
            if (m_data->trackGain)
            {
                for(long j=0; j<frameSize; j++)
                    data[i][j] = int32_t(data[i][j]) * m_data->trackGainValue / std::numeric_limits<int16_t>::max();
            }
        }
        else
        {
            int32_t** data = (int32_t**)outFrame->data;
            for(long j=0; j<frameSize; j++)
                data[i][j] = buffer[i][j];
            if (m_data->trackGain)
            {
                for(long j=0; j<frameSize; j++)
                    data[i][j] = int64_t(data[i][j]) * m_data->trackGainValue / std::numeric_limits<int32_t>::max();
            }
        }
    }
    m_data->position+=frameSize;
    m_data->valid = true;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(
        const FLAC__StreamDecoder *,
        const FLAC__StreamMetadata *metadata,
        void* client_data)
{
    FLACDecoder::private_data *const m_data = (FLACDecoder::private_data*)client_data;
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        m_data->length         = metadata->data.stream_info.total_samples;
        m_data->config.sample_rate    = metadata->data.stream_info.sample_rate;
        m_data->config.sample_width   = metadata->data.stream_info.bits_per_sample;
        m_data->config.channels       = metadata->data.stream_info.channels;
        m_data->max_block_size = metadata->data.stream_info.max_blocksize;

        if (m_data->config.channels <= 2)
            m_data->config.channel_config = aKode::MonoStereo;
        else if (m_data->config.channels <= 7)
            m_data->config.channel_config = aKode::Surround;
        else
            m_data->config.channel_config = aKode::MultiChannel;

        m_data->si = &metadata->data.stream_info;

        m_data->position = 0;

    }
    else if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
    {
        m_data->vc = &metadata->data.vorbis_comment;
        for ( unsigned i=0; i < m_data->vc->num_comments; i++ )
        {
            std::string comment = reinterpret_cast<const char*>(m_data->vc->comments[i].entry);
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
    }
}

static void error_callback(
        const FLAC__StreamDecoder *,
        FLAC__StreamDecoderErrorStatus status,
        void *)
{
    std::cerr << "FLAC error: " << FLAC__StreamDecoderErrorStatusString[status] << "\n";
    switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
            break;
    }
    //data->valid = false;
}

const AudioConfiguration* FLACDecoder::audioConfiguration() {
    return &m_data->config;
}

FLACDecoder::FLACDecoder(File* src) {
    m_data = new private_data;
    m_data->out = 0;
    m_data->decoder = FLAC__stream_decoder_new();
    FLAC__stream_decoder_set_metadata_respond_all(m_data->decoder);
    
    m_data->source = src;
    m_data->source->openRO();
    m_data->source->fadvise();
    // ### check return value
    
    FLAC__stream_decoder_init_stream(
        m_data->decoder,
        flac_read_callback,
        flac_seek_callback,
        flac_tell_callback,
        flac_length_callback,
        eof_callback,
        write_callback,
        metadata_callback,
        error_callback,
        m_data
    );
    
    FLAC__stream_decoder_process_until_end_of_metadata(m_data->decoder);
}

FLACDecoder::~FLACDecoder() {
    FLAC__stream_decoder_finish(m_data->decoder);
    FLAC__stream_decoder_delete(m_data->decoder);
    m_data->source->close();
    delete m_data;
}

bool FLACDecoder::readFrame(AudioFrame* frame) {
    if (m_data->error || m_data->eof) return false;

    if (m_data->out) { // Handle spurious callbacks
        frame->freeSpace();
        *frame = *m_data->out; // copy
        m_data->out->data = 0; // nullify, don't free!
        delete m_data->out;
        m_data->out = 0;
        return true;
    }
    m_data->valid = false;
    m_data->out = frame;
    bool ret = FLAC__stream_decoder_process_single(m_data->decoder);
    m_data->out = 0;
    if (ret && m_data->valid) {
        frame->pos = position();
        return true;
    } else {
        FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(m_data->decoder);
        if (state == FLAC__STREAM_DECODER_END_OF_STREAM)
            m_data->eof = true;
        else
        if (state > FLAC__STREAM_DECODER_END_OF_STREAM)
            m_data->error = true;
        return false;
    }
}

long FLACDecoder::length() {
    float pos = ((float)m_data->length)/m_data->config.sample_rate;
    return (long)(pos*1000.0);
}

long FLACDecoder::position() {
    float pos = ((float)m_data->position)/m_data->config.sample_rate;
    return (long)(pos*1000.0);
}

bool FLACDecoder::eof() {
    return m_data->eof;
}

bool FLACDecoder::error() {
    return m_data->error;
}

bool FLACDecoder::seekable() {
    return m_data->source->seekable();
}

bool FLACDecoder::seek(long pos) {
    if (m_data->error) return false;
    float samplePos = (float)pos * (float)m_data->config.sample_rate / 1000.0;
    m_data->position = (uint64_t)samplePos;
    return FLAC__stream_decoder_seek_absolute(m_data->decoder, m_data->position);
}

class FlacDecoderPlugin : public DecoderPlugin
{
public:
    FlacDecoderPlugin() : DecoderPlugin("flac") { }
    virtual bool canDecode(File* src)
    {
        src->openRO();
        bool o = checkFLAC(src);
        src->close();
        return o;
    }
    virtual FLACDecoder* openDecoder(File* src)
    {
        return new FLACDecoder(src);
    }
} plugin;

} // namespace

namespace aKode
{
DecoderPlugin& flac_decoder()
{
    return plugin;
}

} // namespace


#endif // HAVE_LIBFLAC113
