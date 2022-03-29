#ifndef __AUDIO_DEVS_H__
#define __AUDIO_DEVS_H__

#include <stdio.h>
#include <time.h>
#ifdef AV_OS_WIN32
#include <Windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
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

   int IsDeviceSupportFmt(int device, int channel, int samplerate);

#ifdef AV_OS_WIN32
   static DWORD WINAPI Thread_Playback(LPVOID p);
#else
    static void* Thread_Playback(void* p);
#endif

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

private:
   int m_DevIdx;
   int m_channels;
   int m_samplerate;
   struct SwrContext *m_AudConvertCtx;
   unsigned char* m_TmpBuf;

   unsigned char* m_AudioBuf;
   int m_AudioBufMax;
   int m_AudioBufAvail;
#ifdef AV_OS_WIN32
   CRITICAL_SECTION m_lock;
   HANDLE m_Task;
#else
    pthread_mutex_t m_lock;
    pthread_t m_Task;
#endif

   int TaskLoop;
   int m_IsPaused;
   int m_outChannel;
   int m_outSampleRate;

   //最大缓存音频先关参数, 1000/m_maxCacheFactor = x (ms)
   int m_maxCacheInMs;
};


#endif

