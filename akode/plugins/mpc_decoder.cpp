/*  aKode: Musepack(MPC) Decoder

    Copyright (C) 2004 Allan Sandfeld Jensen <kde@carewolf.com>
    Copyright (C) 2011 Charles Samuels <charles@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <akode/file.h>
#include <akode/audioframe.h>

#include <mpc/mpcdec.h>

#include <limits>

#include "mpc_decoder.h"

using namespace aKode;

namespace
{

namespace mr
{
static mpc_int32_t read(mpc_reader *r, void *ptr, mpc_int32_t size)
{
	aKode::File *f = (aKode::File*)r->data;
	return f->read( (char*)ptr, size);
}

mpc_bool_t seek(mpc_reader *r, mpc_int32_t offset)
{
	aKode::File *f = (aKode::File*)r->data;
	return f->seek(offset);
}

mpc_int32_t tell(mpc_reader *r)
{
	aKode::File *f = (aKode::File*)r->data;
	return f->position();
}

mpc_int32_t get_size(mpc_reader *r)
{
	aKode::File *f = (aKode::File*)r->data;
	return f->length();
}

mpc_bool_t canseek(mpc_reader *r)
{
	aKode::File *f = (aKode::File*)r->data;
	return f->seekable();
}

}

class MPCDecoder : public Decoder
{
public:
	MPCDecoder(File* src);
	virtual ~MPCDecoder();

	void initialize();
	
	virtual bool readFrame(AudioFrame* frame);
	virtual long length();
	virtual long position();
	virtual bool seek(long);
	virtual bool seekable();
	virtual bool eof();
	virtual bool error();

	virtual const AudioConfiguration* audioConfiguration();

private:
	AudioConfiguration config;
	mpc_reader reader;
	mpc_streaminfo si;
	mpc_demux* demux;
	
	long mPos;

	bool mEof;
	bool mError;

	MPC_SAMPLE_FORMAT *buffer;
};

MPCDecoder::MPCDecoder(File *src)
{
	mError = false;
	mEof = false;
	demux=0;
	buffer = 0;
	mPos = 0;

	reader.read = mr::read;
	reader.seek = mr::seek;
	reader.tell = mr::tell;
	reader.get_size = mr::get_size;
	reader.canseek = mr::canseek;
	reader.data = src;
}

MPCDecoder::~MPCDecoder()
{
	delete[] buffer;
	if (demux)
		mpc_demux_exit(demux);
}

void MPCDecoder::initialize()
{
	demux = mpc_demux_init(&reader);
	
	if(!demux)
	{
		mError = true;
		return;
	}
	
	mpc_demux_get_info(demux,  &si);
	
	config.channels = si.channels;
	config.sample_rate = si.sample_freq;
	config.sample_width = 16;

	if (config.channels <=2)
		config.channel_config = MonoStereo;
	else
		config.channel_config = MultiChannel;
}

bool MPCDecoder::readFrame(AudioFrame* frame)
{
	if (!demux)
	{
		initialize();
		if (!demux)
			return false;
		buffer = new MPC_SAMPLE_FORMAT[MPC_DECODER_BUFFER_LENGTH];
	}
	
	mpc_frame_info mpcframe;
	mpcframe.buffer = buffer;
	const mpc_status status = mpc_demux_decode(demux, &mpcframe);
	if (mpcframe.bits < 0)
	{
		mEof = true;
		return false;
	}

	mPos += mpcframe.samples;
	
	int channels = config.channels;
	int length = mpcframe.samples;
	frame->reserveSpace(&config, length);

	uint16_t** data = (uint16_t**)frame->data;
	for(int i=0; i<length; i++)
		for(int j=0; j<channels; j++)
		{
			data[j][i] = buffer[i*channels+j] * std::numeric_limits<int16_t>::max();
		}

	frame->pos = position();
	return true;
}

long MPCDecoder::length()
{
	if (!demux) return -1;
	return mpc_streaminfo_get_length(&si)*1000;
}

long MPCDecoder::position()
{
	if (!demux) return -1;
	return mPos/config.sample_rate * 1000;
}

bool MPCDecoder::eof()
{
	return mEof;
}

bool MPCDecoder::error()
{
	return mError;
}

bool MPCDecoder::seekable()
{
	return reader.canseek(&reader);
}

bool MPCDecoder::seek(long pos)
{
	if (!demux) return -1;

	const mpc_status status = mpc_demux_seek_second(demux, pos/1000);
	if (MPC_STATUS_OK == status)
	{
		uint64_t x= pos;
		x *= config.sample_rate;
		mPos = x/1000;
		return true;
	}
	else
		return false;
}

const AudioConfiguration* MPCDecoder::audioConfiguration()
{
    return &config;
}

class MpcDecoderPlugin : public DecoderPlugin
{
public:
	MpcDecoderPlugin() : DecoderPlugin("mpc") { }
	virtual bool canDecode(File* src)
	{
		src->openRO();
		MPCDecoder dec(src);
		dec.initialize();
		bool x = !dec.error();
		src->close();
		return x;
	}
	virtual MPCDecoder* openDecoder(File* src)
	{
		src->openRO();
		src->fadvise();
		return new MPCDecoder(src);
	}
} plugin;

} // namespace

namespace aKode
{
DecoderPlugin& mpc_decoder()
{
	return plugin;
}

} // namespace


// kate: space-indent off; replace-tabs off;

//#endif
