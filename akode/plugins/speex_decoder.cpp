//#ifdef AKODE_WITH_SPEEX

/*  aKode: Speex-Decoder

    Copyright (C) 2014 Charles Samuels <charles@kde.org>

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

#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <speex/speex_callbacks.h>

#include <ogg/ogg.h>

#include <akode/file.h>
#include <akode/audioframe.h>
#include <akode/decoder.h>
#include "speex_decoder.h"

#include <cstring>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

using namespace aKode;

namespace
{

class SpeexDecoder : public Decoder
{
	File *const file;
	AudioConfiguration config;

	void *dec_state=nullptr;
	bool init=false;
	
	// Speex properties
	SpeexBits bits;
	SpeexHeader *header=nullptr;
	SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;
	SpeexMode mode;
	int mLength=-1, mPosition=0;
	
	int pageStream=-1;
	int packetCount=0;
	
	ogg_sync_state oy;
	ogg_stream_state os;
	ogg_page og;
	
	static const unsigned bufferLen=200;
	spx_int16_t buffer[2000];
	
public:
	SpeexDecoder(File* src)
		: file(src)
	{
		speex_bits_init(&bits);
		ogg_sync_init(&oy);
	}
	virtual ~SpeexDecoder()
	{
		speex_bits_destroy(&bits);
		if (dec_state)
			speex_decoder_destroy(dec_state);
		if (header)
			free(header);
	}
	
	bool hasHeader() const { return !!header; }

	virtual bool openFile()
	{
		file->openRO();
		{
			// create another decoder object to get the length for us
			SpeexDecoder dec(file);
			//dec.sync();
			
			auto nothing = [](const ogg_packet&) { };

			while (!dec.eof())
				dec.read(nothing);
			
			mLength = dec.position();
		}
		
		file->close();
		file->openRO();
		file->fadvise();
		
		return true;
	}

	bool hasPage=false;
	int speexSerialNo=-1;
	int lookahead=0;
	
	template<class DecoderFn>
	void read(const DecoderFn& fn)
	{
		while (1)
		{
			if (!hasPage)
			{
				// construct a page
				while (ogg_sync_pageout(&oy, &og) != 1)
				{
					char *const data = ogg_sync_buffer(&oy, bufferLen);
					const long readSize = file->read(data, bufferLen);
					if (readSize == 0)
						return;
					ogg_sync_wrote(&oy, readSize);
				}
				hasPage=true;
				
				// now I have a page
				if (!init)
				{
					ogg_stream_init(&os, pageStream = ogg_page_serialno(&og));
					init = true;
				}
				
				if (ogg_page_serialno(&og) != os.serialno)
				{
					// read all streams
					ogg_stream_reset_serialno(&os, ogg_page_serialno(&og));
				}
				if (-1 == ogg_stream_pagein(&os, &og))
					throw std::runtime_error("error reading page");
				
			}
			
			
			// decode one packet
			ogg_packet op;
			
			{
				int result = ogg_stream_packetout(&os, &op);
				if (result == -1)
					continue;
				else if (result == 0)
				{
					// need a new page
					hasPage = false;
					continue;
				}
				
				if (op.bytes >=5 && std::memcmp(op.packet, "Speex", 5)==0)
				{
					speexSerialNo = os.serialno;
				}
				if (speexSerialNo == -1 || os.serialno != speexSerialNo)
				{
					std::cerr << "Bad stream" << std::endl;
					continue;
				}
			}
			
			// beginning of a logical stream
			if (packetCount == 0)
			{
				if (header)
					free(header);
				header = speex_packet_to_header((char*)op.packet, op.bytes);
				if (header)
				{
					config.channels = header->nb_channels;
					config.channel_config = MonoStereo;
					config.surround_config = 0;
					config.sample_width = 16;
					config.sample_rate = header->rate;
					
					if (header->mode >= SPEEX_NB_MODES || header->mode<0)
					{
						throw std::runtime_error("Invalid mode");
					}
					
					const SpeexMode *mode = speex_lib_get_mode(header->mode);
					dec_state  = speex_decoder_init(mode);
					if (!(header->nb_channels==1))
					{
						config.channels = 2;
						SpeexCallback callback;
						callback.callback_id = SPEEX_INBAND_STEREO;
						callback.func = speex_std_stereo_request_handler;
						callback.data = &stereo;
						speex_decoder_ctl(dec_state, SPEEX_SET_HANDLER, &callback);
					}
					speex_decoder_ctl(dec_state, SPEEX_GET_LOOKAHEAD, &lookahead);
				}
			}
			else if (packetCount==1)
			{
			}
			else if (packetCount<= 1 + header->extra_headers)
			{
				// more comments
			}
			else if (op.e_o_s)
			{
				// end of a stream (not necessarily the file, though)
				free(header);
				header = nullptr;
				speex_decoder_destroy(dec_state);
				dec_state=nullptr;
			}
			else
			{ // finally, we can read some data
				mPosition = ogg_page_granulepos(&og)*1000/ header->rate;
				fn(op);
				packetCount++;
				return;
			}
			
			packetCount++;
		}
	}
		
	virtual bool readFrame(AudioFrame* frame)
	{
		auto decoder =
		[&](const ogg_packet& op)
		{
			speex_bits_read_from(&bits, reinterpret_cast<char*>(op.packet), op.bytes);
			int frame_size;
			speex_encoder_ctl(dec_state,SPEEX_GET_FRAME_SIZE, &frame_size);  
			
			frame->reserveSpace(&config, long(frame_size*header->frames_per_packet));

			// For multiple frames within packets
			for (int packetn=0; packetn < header->frames_per_packet; packetn++)
			{
				int ret = speex_decode_int(dec_state, &bits, buffer);
				if (ret == -1 || ret == -2 || speex_bits_remaining(&bits) < 0)
				{
					return;
				}
				
				if (config.channels == 2)
				{
					speex_decode_stereo_int(buffer, frame_size, &stereo);
				}
				
				// Converting and clipping check
				int16_t** data = (int16_t**)frame->data;
				for (int i = lookahead; i < frame_size; i++)
					for (int j=0; j < header->nb_channels; j++)
						data[j][i+packetn] = buffer[i*header->nb_channels+j];
				lookahead = 0;
				frame->pos = mPosition;
			}
		};
		
		frame->length=0;
		read(decoder);
		return !eof();
	}
	
	virtual long length()
	{
		return mLength;
	}
	
	virtual long position()
	{
		//std::cerr << "p: " << mPosition << std::endl;
		return mPosition;
	}
	virtual bool seek(long to)
	{
		std::cerr << "trying to seek to " << to << std::endl;
		if (to < position())
		{
			file->seek(0);
			mPosition=0;
			hasPage=false;
		}
		auto nothing = [](const ogg_packet&) { };
		while (position() < to)
		{
			read(nothing);
			hasPage=false;
		}
		
		return true;
	}
	virtual bool seekable() { return true; }
	virtual bool eof() { return !hasPage && file->eof(); }
	virtual bool error() { return false; }

	virtual const AudioConfiguration* audioConfiguration()
	{
		return &config;
	}
};

class SpeexDecoderPlugin: public DecoderPlugin
{
public:
	SpeexDecoderPlugin() : DecoderPlugin("speex") { }
	virtual bool canDecode(File* src)
	{
		src->openRO();
		SpeexDecoder dec(src);
		auto nothing =
			[](const ogg_packet&) { };
		for (unsigned tries=0; tries < 10; tries++)
		{
			dec.read(nothing);
		}
		
		src->close();
		return dec.hasHeader();
	}
	virtual SpeexDecoder* openDecoder(File* src)
	{
		SpeexDecoder *d = new SpeexDecoder(src);
		d->openFile();
		return d;
	}
} plugin;

} // namespace

namespace aKode
{
DecoderPlugin& speex_decoder()
{
    return plugin;
}

} // namespace


//#endif
