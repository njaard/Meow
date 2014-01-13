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
#include <dsound.h>
#include <windows.h>

#include <fstream>

#include "dsound_sink.h"

#include <qbytearray.h>
#include <qstring.h>

using namespace aKode;

static HMODULE dll_=nullptr;
typedef HRESULT (WINAPI *DLLDirectSoundEnumerateW)(LPDSENUMCALLBACKW, LPVOID);
typedef HRESULT (WINAPI *DLLDirectSoundCreate8)(LPCGUID lpGUID,LPDIRECTSOUND8 *ppDS8,LPUNKNOWN pUnkOuter);


static DLLDirectSoundEnumerateW dllDirectSoundEnumerateW=nullptr;
static DLLDirectSoundCreate8 dllDirectSoundCreate8=nullptr;


static HMODULE dll()
{
    if (!dll_)
    {
        dll_ = LoadLibrary("DSOUND.DLL");
        dllDirectSoundEnumerateW
            = (DLLDirectSoundEnumerateW)GetProcAddress( dll_,
                "DirectSoundEnumerateW" );
        dllDirectSoundCreate8
            = (DLLDirectSoundCreate8)GetProcAddress( dll_,
                "DirectSoundCreate8" );
        // do this once so dsound doesn't ever get unloaded by COM
        LPDIRECTSOUND8 lpds;
        HRESULT r = dllDirectSoundCreate8(
            0,
            &lpds,
            0
        );
        
    }
    return dll_;
}



namespace
{

class DSoundSink : public Sink
{
public:
    DSoundSink(const std::string &device);
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
    std::string device;
    int myChannels=2;
    
    int writingInBlocksOf; // samples
    
    HANDLE threadH;

    bool error;
    
    UINT writingAt, bufferSize;
};

DSoundSink::DSoundSink(const std::string &device)
{
    d = new private_data;
    d->device = device;
    d->threadH = 0;
    d->writingAt =0;
    d->dsBuffer=0;
}

DSoundSink::~DSoundSink()
{
    close();
    if (d->lpds)
        d->lpds->Release();
    delete d;
}

static std::string hresultToString(HRESULT r)
{
    if (r == DSERR_ALLOCATED)
        return "allocated";
    else if (r == DSERR_INVALIDPARAM)
        return "invalidparam";
    else if (r == DSERR_NOAGGREGATION)
        return "noaggregation";
    else if (r == DSERR_NODRIVER)
        return "nodriver";
    else if (r == DSERR_OUTOFMEMORY)
        return "outofmemory";
    else
        return "ok";
}

bool DSoundSink::open()
{
    dll();
    
    LPGUID g=nullptr;
    QByteArray a = QByteArray::fromHex(QByteArray(d->device.c_str(), d->device.size()));
    if (!d->device.empty())
        g = (LPGUID)a.data();
        
    HRESULT r = dllDirectSoundCreate8(
          g,
          &d->lpds,
          0
      );
    d->error = r!=DS_OK;
    if (d->error)
    {
        std::cerr << "DirectSoundCreate8: error code " << hresultToString(r) << " guid=" << g << std::endl;
        return false;
    }
    // this works, I don't know why.
    d->lpds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
    
    return !d->error;
}

void DSoundSink::close()
{
    if (d->dsBuffer)
        d->dsBuffer->Release();
    d->dsBuffer=0;
}

int DSoundSink::setAudioConfiguration(const AudioConfiguration* config)
{
    close();
    if (d->error) return -1;
    d->config = *config;
    int exact = 0;

    if (config->sample_width!=16)
    {
        d->config.sample_width = 16;
        exact = 1;
    }
    if (config->sample_rate!=44100)
    {
        d->config.sample_rate = 44100;
        exact = 1;
    }

    WAVEFORMATEX wfx; 
    memset(&wfx, 0, sizeof(WAVEFORMATEX)); 
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.wBitsPerSample = d->config.sample_width;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = d->config.sample_rate; 
    wfx.nBlockAlign = 4; 
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; 
    
    DSBUFFERDESC bd;
    memset (&bd, 0, sizeof (DSBUFFERDESC));
    bd.dwSize = sizeof (DSBUFFERDESC);
    bd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    d->bufferSize = bd.dwBufferBytes = wfx.nAvgBytesPerSec/4;
    
    bd.lpwfxFormat = &wfx;
    
    d->writingInBlocksOf = bd.dwBufferBytes/wfx.wBitsPerSample/wfx.nChannels/4;
    
    std::cerr << "lpds=" << d->lpds << std::endl;
    if (!SUCCEEDED(d->lpds->CreateSoundBuffer(&bd, &d->dsBuffer, NULL)))
    {
         MessageBox(0, "failed to create sound buffer", "failure", MB_OK);
    }
    
    resume();

    return exact;
}

const AudioConfiguration* DSoundSink::audioConfiguration() const
{
    return &d->config;
}



template<class T>
bool DSoundSink::_writeFrame(AudioFrame* frame)
{
	std::cerr << "writing frame" << std::endl;
    const int channels = d->myChannels;
    const int inChannels = frame->channels;
    const T*const * data = (T**)frame->data;
    
    int atSample=0;
    
    // bps=4
    const UINT bytesPerSample = d->config.sample_width/8*channels;
    if (sizeof(T) != d->config.sample_width/8)
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
        if (DS_OK != d->dsBuffer->GetCurrentPosition(&reading, 0))
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
        dsresult = d->dsBuffer->Lock(
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
                    out[i*channels + j] = data[std::min(j,inChannels-1)][atSample];
                atSample++;
            }
        }
        {
            T *const out = (T*)p_wrap_around;
            for (unsigned long i=0; i < l_bytes2/bytesPerSample; i++)
            {
                for(int j=0; j<channels; j++)
                    out[i*channels + j] = data[std::min(j,inChannels-1)][atSample];
                atSample++;
            }
        }
        
        d->writingAt = (d->writingAt + l_bytes1 + l_bytes2) % d->bufferSize;
        
        d->dsBuffer->Unlock(
                p_write_position, l_bytes1,
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
    d->dsBuffer->Stop();
}

// Do not confuse this with snd_pcm_resume which is used to resume from a suspend
void DSoundSink::resume()
{
    HRESULT x = d->dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
    if (x == DSERR_BUFFERLOST)
        d->dsBuffer->Restore();
    x = d->dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
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
    virtual std::shared_ptr<Sink> openSink(const std::string &device)
    {
        return std::make_shared<DSoundSink>(device);
    }
    virtual std::vector<std::pair<std::string, std::string> > deviceNames()
    {
        dll();
                       
        dllDirectSoundEnumerateW(callback_, this);
        std::vector<std::pair<std::string, std::string> > s;
        std::swap(s, result);
        return s;
    }
private:
    static CALLBACK BOOL callback_(LPGUID guid, LPCWSTR desc, LPCWSTR module, LPVOID context)
    {
        return ((DSoundSinkPlugin*)context)->callback(guid, desc, module);
    }
    std::vector<std::pair<std::string,std::string> > result;
    CALLBACK BOOL callback(LPGUID guid, LPCWSTR desc_, LPCWSTR module)
    {
    
        std::string desc = QString::fromUtf16((const ushort*)desc_).toUtf8().constData();
        if (guid)
        {
            std::string g = QByteArray((const char*)guid, sizeof(GUID)).toHex().constData();
            
            result.push_back( { g, desc} );
        }
        return true;
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


