#ifndef __AUDIO_DEVS_H__
#define __AUDIO_DEVS_H__

#include "uv.h"
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "portaudio.h"
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

class PlaybackDev
{
public:
   PlaybackDev();
   ~PlaybackDev();

   static int GetDeviceFmt(int device, int &channel, int &samplerate);
   static int IsDeviceSupportFmt(int device, int channel, int samplerate);
   static void ThreadPlayback(void* p);

   void SetOutputFormat(int channels, int samplerate);
   void SetMaxDelayMs (int value);

   int PlayStart(int DiviceIdx, int channels, int samplerate);
   int PlayStop();
   int PushData(unsigned char * pData, int FrameCnt);
   int Pause();
   int Resume();
   int Flush();

private:
    int openAudioDev(PaStream **stream);
    int closeAudioDev(PaStream *stream);
    int pushPlayData(unsigned char * pData, int size);
    int readPlayData(unsigned char * pData, int size);

private:
   int m_devIdx;
   int m_channels;
   int m_samplerate;
   struct SwrContext *m_audConvertCtx;
   unsigned char* m_TmpBuf;

   unsigned char* m_audioBuf;
   long m_audioBufMax;
   long m_audioBufAvail;
   long m_audioBufWp;
   long m_audioBufRp;

   uv_thread_t m_Task;

   int m_flushFlag;
   int TaskLoop;
   int m_IsPaused;
   int m_outChannel;
   int m_outSampleRate;

   //最大缓存音频先关参数, 1000/m_maxCacheFactor = x (ms)
   int m_maxCacheInMs;

   FILE* m_fileDebug;
};


#endif

