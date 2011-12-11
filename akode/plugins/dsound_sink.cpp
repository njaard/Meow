/*  aKode: ALSA Sink

    Copyright (C) 2011 Charles Samueks <charles@kde.org>

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

#include <iostream>

#include <akode/audioframe.h>
#include "dsound.h"
#include <windows.h>

#include <fstream>

#include "dsound_sink.h"

using namespace aKode;

namespace
{

class DSoundSink : public Sink {
public:
    DSoundSink();
    ~DSoundSink();
    bool open();
    void close();
    int setAudioConfiguration(const AudioConfiguration *config);
    const AudioConfiguration* audioConfiguration() const;
    // Writes blocking
    bool writeFrame(AudioFrame *frame);
    void pause();
    void resume();

    struct private_data;
private:
    template<class T> bool _writeFrame(AudioFrame *frame);
#ifdef DWORD
    static DWORD _writerThread(void *);
#endif
    void writerThread();
    
    private_data *d;
};

struct DSoundSink::private_data
{
    private_data() {}

    LPDIRECTSOUND8 lpds;
    LPDIRECTSOUNDBUFFER dsBuffer;

    AudioConfiguration config;
    
    int writingInBlocksOf; // samples
    
    HANDLE threadH;

    bool error;
    
    UINT writingAt, bufferSize;
};

DSoundSink::DSoundSink()
{
    d = new private_data;
    d->threadH = 0;
    d->writingAt =0;
    d->dsBuffer=0;
}

DSoundSink::~DSoundSink()
{
    close();
    delete d;
}

bool DSoundSink::open()
{
    HRESULT r = DirectSoundCreate8(
          0,
          &d->lpds,
          0
      );
    // this works, I don't know why.
    IDirectSound_SetCooperativeLevel(d->lpds, GetDesktopWindow() , DSSCL_EXCLUSIVE);
    d->error = r!=DS_OK;
    
    return !d->error;
}

void DSoundSink::close()
{
    if (d->dsBuffer)
        IDirectSoundBuffer_Release(d->dsBuffer);
    d->dsBuffer=0;
}

int DSoundSink::setAudioConfiguration(const AudioConfiguration* config)
{
    close();
    if (d->error) return -1;
    if (config->sample_width!=16)
        return -1;
    if (config->channels!=2)
        return -1;
    if (config->sample_rate!=44100)
        return -1;

    WAVEFORMATEX wfx; 
    memset(&wfx, 0, sizeof(WAVEFORMATEX)); 
    if (config->sample_width==16)
    {
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample = 16;
    }
    wfx.nChannels = 2; 
    wfx.nSamplesPerSec = 44100; 
    wfx.nBlockAlign = 4; 
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; 
    
    DSBUFFERDESC bd;
    memset (&bd, 0, sizeof (DSBUFFERDESC));
    bd.dwSize = sizeof (DSBUFFERDESC);
    bd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    d->bufferSize = bd.dwBufferBytes = wfx.nAvgBytesPerSec/4;
    
    bd.lpwfxFormat = &wfx;
    
    d->writingInBlocksOf = bd.dwBufferBytes/wfx.wBitsPerSample/wfx.nChannels/4;
    
    if (!SUCCEEDED(d->lpds->CreateSoundBuffer(&bd, &d->dsBuffer, NULL)))
    {
         MessageBox(0, "failed to create sound buffer", "failure", MB_OK);
    }
    
    resume();

    d->config = *config;
    return 0;
}

const AudioConfiguration* DSoundSink::audioConfiguration() const
{
    return &d->config;
}



template<class T>
bool DSoundSink::_writeFrame(AudioFrame* frame)
{
    const int channels = d->config.channels;
    const T*const * data = (T**)frame->data;
    
    int atSample=0;
    
    // bps=4
    const UINT bytesPerSample = frame->sample_width/8*frame->channels;
    if (sizeof(T) != frame->sample_width/8)
    {
        std::cerr << "wrong sample width" << std::endl;
        return false;
    }
    
    DWORD lastCursor;
    
    while (atSample < frame->length)
    {
        DWORD wantBytes = (frame->length-atSample)*bytesPerSample;
        
        HRESULT dsresult;
        DWORD reading;
        if (DS_OK != IDirectSoundBuffer_GetCurrentPosition(d->dsBuffer, &reading, 0))
            continue;
        DWORD avail = (reading-d->writingAt+d->bufferSize) % d->bufferSize;
        wantBytes = std::min<DWORD>(wantBytes, avail);
        
        if (wantBytes == 0)
        {
            Sleep(25);
            continue;
        }

        void *p_write_position, *p_wrap_around;
        unsigned long l_bytes1, l_bytes2;
        dsresult = IDirectSoundBuffer_Lock(
                d->dsBuffer,
                d->writingAt,
                wantBytes,
                &p_write_position,
                &l_bytes1,
                &p_wrap_around,           // Buffer address (if wrap around)
                &l_bytes2,               // Count of bytes after wrap around
                0
            );
        if( dsresult != DS_OK )
        {
            std::cerr << "buffer lost" <<std::endl;
            continue;
        }
        
        {
            T *const out = (T*)p_write_position;
            for (unsigned long i=0; i < l_bytes1/bytesPerSample; i++)
            {
                for(int j=0; j<channels; j++)
                    out[i*channels + j] = data[j][atSample];
                atSample++;
            }
        }
        {
            T *const out = (T*)p_wrap_around;
            for (unsigned long i=0; i < l_bytes2/bytesPerSample; i++)
            {
                for(int j=0; j<channels; j++)
                    out[i*channels + j] = data[j][atSample];
                atSample++;
            }
        }
        
        d->writingAt = (d->writingAt + l_bytes1 + l_bytes2) % d->bufferSize;
        
        IDirectSoundBuffer_Unlock(
                d->dsBuffer, p_write_position, l_bytes1,
                p_wrap_around, l_bytes2
            );
    }
    
    return true;
}


bool DSoundSink::writeFrame(AudioFrame* frame)
{
    return _writeFrame<int16_t>(frame);
/*    if (frame->sample_width<0)
        return _writeFrame<float>(frame);
    else if (frame->sample_width<=8)
        return _writeFrame<int8_t>(frame);
    else if (frame->sample_width<=16) */
//    else if (frame->sample_width<=32)
//        return _writeFrame<int32_t>(frame);

    return false;
}

void DSoundSink::pause()
{
    IDirectSoundBuffer_Stop(d->dsBuffer);
}

// Do not confuse this with snd_pcm_resume which is used to resume from a suspend
void DSoundSink::resume()
{
    HRESULT x = IDirectSoundBuffer_Play(d->dsBuffer, 0, 0, DSBPLAY_LOOPING);
    if (x == DSERR_BUFFERLOST)
        IDirectSoundBuffer_Restore( d->dsBuffer );
    x = IDirectSoundBuffer_Play(d->dsBuffer, 0, 0, DSBPLAY_LOOPING);
    if (x == DSERR_BUFFERLOST)
         MessageBox(0, "failed to play sound buffer, bufferlost", "failure", MB_OK);
    else if (x == DSERR_INVALIDCALL)
         MessageBox(0, "failed to play sound buffer, DSERR_INVALIDCALL", "failure", MB_OK);
    else if (x == DSERR_INVALIDPARAM)
         MessageBox(0, "failed to play sound buffer, DSERR_INVALIDPARAM", "failure", MB_OK);
    else if (x == DSERR_PRIOLEVELNEEDED)
         MessageBox(0, "failed to play sound buffer, DSERR_PRIOLEVELNEEDED", "failure", MB_OK);
}

class DSoundSinkPlugin : public SinkPlugin
{
public:
    DSoundSinkPlugin() : SinkPlugin("dsound") { }
    virtual DSoundSink* openSink()
    {
        return new DSoundSink();
    }
} plugin;

} // namespace

namespace aKode
{
SinkPlugin& dsound_sink()
{
    return plugin;
}
}


