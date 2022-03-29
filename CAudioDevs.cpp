extern "C"{

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
# include <stdint.h>
#endif

}
#include "CAudioDevs.h"
#include "anLogs.h"

PlaybackDev::PlaybackDev()
{
   m_DevIdx = 0;
   m_AudConvertCtx = NULL;
   m_TmpBuf = NULL;
   m_IsPaused = 0;
   m_outChannel = 2;
   m_outSampleRate = 44100;
   TaskLoop = 0;
#ifdef AV_OS_WIN32
   InitializeCriticalSection(&m_lock);
#else
   pthread_mutex_init(&m_lock, NULL);
#endif

   m_Task = NULL;
   m_maxCacheInMs = 200;
   //缓冲区只控制200ms
   m_AudioBufMax = (m_outSampleRate * m_outChannel * 2 * m_maxCacheInMs) / 1000;
   m_AudioBufAvail = 0;
   m_AudioBuf = (unsigned char*)malloc(m_AudioBufMax);
}

int PlaybackDev::openAudioDev(PaStream **stream)
{
    int err;
    const PaDeviceInfo *devinfo;
    PaStreamParameters playParam;
    PaStream *stream_new = NULL;

    *stream = NULL;

    playParam.device = m_DevIdx;
    playParam.channelCount = m_outChannel;
    playParam.sampleFormat = paInt16;
    playParam.hostApiSpecificStreamInfo = NULL;

    if (Pa_GetDeviceInfo(playParam.device) == NULL)
    {
        return -1;
    }

    playParam.suggestedLatency = Pa_GetDeviceInfo(playParam.device)->defaultLowOutputLatency;

    devinfo = Pa_GetDeviceInfo(playParam.device);

    err = Pa_OpenStream(
        &stream_new,
        NULL,
        &playParam,
        m_outSampleRate,
        m_outSampleRate / (m_outChannel * 5), //20ms perframe
        paNoFlag,
        NULL,
        NULL);
    if (err != paNoError)
    {
        fprintf(stderr, "Pa_OpenStream Failed.\n");
        return -1;
    }

    err = Pa_StartStream(stream_new);
    if (err != paNoError)
    {
        Pa_CloseStream(stream_new);
        fprintf(stderr, "Pa_StartStream Failed.\n");
        return -1;
    }

    *stream = stream_new;

    return 0;
}
int PlaybackDev::closeAudioDev(PaStream *stream)
{
    if (stream)
    {
        Pa_CloseStream(stream);
        stream = NULL;
    }
    return 0;
}
#ifdef AV_OS_WIN32
DWORD WINAPI PlaybackDev::Thread_Playback(LPVOID p)
#else
void* PlaybackDev::Thread_Playback(void* p)
#endif
{
    PlaybackDev* pobjs = (PlaybackDev *)p;
    PaStream *stream = NULL;
    unsigned char * writeTmp; //48K也有100ms，足够了
    int writeAudioUnit;
    int resume_size;
    int left_size_wait = 0;

    //我们一次写入100ms，确保延迟
    writeAudioUnit = (pobjs->m_outSampleRate * pobjs->m_outChannel * 2) / 10;

    writeTmp = (unsigned char*)calloc(1, writeAudioUnit);

    while (pobjs->TaskLoop && writeTmp)
    {

        if (stream == NULL)
        {
            if (pobjs->openAudioDev(&stream) == 0)
            {
                //打开成功，需要情况缓冲区
            }
#ifdef AV_OS_WIN32
            Sleep(50);
#else
            usleep(50000);
#endif
            continue;
        }

        if (pobjs->m_AudioBufAvail < writeAudioUnit)
        {
            //统计数据不够到底多少次了
            if (pobjs->m_AudioBufAvail > 0)
            {
                left_size_wait++;
            }

            //100ms以内，我等你
            if (left_size_wait < 5)
            {
#ifdef AV_OS_WIN32
                Sleep(20);
#else
                usleep(20000);
#endif
                continue;
            }

        }

        left_size_wait = 0;
        resume_size = 0;

#ifdef AV_OS_WIN32
        EnterCriticalSection(&(pobjs->m_lock));
#else
        pthread_mutex_lock(&(pobjs->m_lock));
#endif
        if (pobjs->m_AudioBufAvail >= writeAudioUnit)
        {
            memcpy(writeTmp, pobjs->m_AudioBuf, writeAudioUnit);
            //移动缓冲区
            memmove(pobjs->m_AudioBuf, pobjs->m_AudioBuf + writeAudioUnit, pobjs->m_AudioBufAvail - writeAudioUnit);
            pobjs->m_AudioBufAvail -= writeAudioUnit;
            resume_size = writeAudioUnit;
        }
        else
        {
            resume_size = pobjs->m_AudioBufAvail;
            if (resume_size > 0)
            {
               memcpy(writeTmp, pobjs->m_AudioBuf, resume_size);
            }
            pobjs->m_AudioBufAvail = 0;
        }
#ifdef AV_OS_WIN32
        LeaveCriticalSection(&(pobjs->m_lock));
#else
        pthread_mutex_unlock(&(pobjs->m_lock));
#endif
        if (resume_size >0)
        {
            Pa_WriteStream(stream, writeTmp, resume_size / (2 * pobjs->m_outChannel));
        }
    }

    if (stream)
    {
        pobjs->closeAudioDev(stream);
        stream = NULL;
    }

    if (writeTmp)
    {
        free(writeTmp);
        writeTmp = NULL;
    }
    
    return 0;
}

PlaybackDev::~PlaybackDev()
{
    PlayStop();
#ifdef AV_OS_WIN32
    DeleteCriticalSection(&m_lock);
#else
    pthread_mutex_destroy(&m_lock);
#endif
}

void PlaybackDev::SetMaxDelayMs(int value)
{
    if (m_maxCacheInMs > 0)
    {
        m_maxCacheInMs = value;
    }
}

void PlaybackDev::SetOutputFormat(int channels, int samplerate)
{
    m_outChannel = channels;
    m_outSampleRate = samplerate;
    if (m_AudioBufMax == NULL)
    {
        free(m_AudioBuf);
        m_AudioBufMax = NULL;
    }

    m_AudioBufMax = (m_outSampleRate * m_outChannel * 2 * m_maxCacheInMs) / 1000;
    m_AudioBuf = (unsigned char*)malloc(m_AudioBufMax);

}
int PlaybackDev::PlayStop()
{
    TaskLoop = 0;
    if (m_Task)
    {
#ifdef AV_OS_WIN32
        WaitForSingleObject(m_Task, INFINITE);
        CloseHandle(m_Task);
        m_Task = NULL;
#else
        pthread_join(m_Task, NULL);
        m_Task = 0;
#endif
    }
   if (m_AudConvertCtx)
   {
      swr_free(&m_AudConvertCtx);
      m_AudConvertCtx = NULL;
   }

   if (m_TmpBuf)
   {
      free(m_TmpBuf);
      m_TmpBuf = NULL;
   }

   return 0;
}

int PlaybackDev::PlayStart(int DiviceIdx, int channels, int samplerate)
{
   int layout;
   int layoutDst;

   PlayStop();

   m_IsPaused = 0;
   m_AudioBufAvail = 0;
   m_channels = channels;
   m_samplerate = samplerate;
 
   if (DiviceIdx >= 0)
   {
      m_DevIdx = DiviceIdx;
   }
	else
   {
      m_DevIdx = Pa_GetDefaultOutputDevice();
   }

   if (m_DevIdx < 0)
   {
      return -1;
   }

   if (IsDeviceSupportFmt(m_DevIdx, m_outChannel, m_outSampleRate) == 0)
   {
       //如果不支持，我们就用默认的格式播放
       const PaDeviceInfo* devinfo = Pa_GetDeviceInfo(m_DevIdx);
       if (devinfo)
       {
           int out_channels = devinfo->maxOutputChannels;
           int out_samplerate = (int)(devinfo->defaultSampleRate);

           if (out_channels > 2)
           {
               out_channels = 2;
           }

           if (out_channels < 1)
           {
               return -1;
           }

           SetOutputFormat(out_channels, out_samplerate);
       }
       else
       {
           //没有合适的格式来做
           return -1;
       }
   }


   if (m_channels == 1)
   {
      layout = AV_CH_LAYOUT_MONO;
   }
   else
   {
      layout = AV_CH_LAYOUT_STEREO;
   }

   if (m_outChannel == 1)
   {
      layoutDst = AV_CH_LAYOUT_MONO;
   }
   else
   {
      layoutDst = AV_CH_LAYOUT_STEREO;
   }

   //足够200ms采样的数据
   m_TmpBuf = (unsigned char*)malloc( (m_outSampleRate*m_outChannel*2) / 5);

   //should redo here
   if ((m_samplerate != m_outSampleRate) || (m_outChannel != m_channels))
   {
      m_AudConvertCtx = swr_alloc_set_opts(m_AudConvertCtx,
         layoutDst, AV_SAMPLE_FMT_S16, m_outSampleRate, //dst
         layout, AV_SAMPLE_FMT_S16, m_samplerate, //src 
         0, NULL);
      swr_init(m_AudConvertCtx);
   }
   else
   {
      m_AudConvertCtx = NULL;
   }

   TaskLoop = 1;
#ifdef AV_OS_WIN32
   m_Task = ::CreateThread(NULL, 0, PlaybackDev::Thread_Playback, this, 0, NULL);
   if (m_Task == NULL)
#else
    m_Task = 0;
    pthread_create(&m_Task, 0, PlaybackDev::Thread_Playback, this);
    if (m_Task == 0)
#endif
   {
       fprintf(stderr, "CreateThread Failed.\n");
       TaskLoop = 0;
   }

   return 0;
}
int PlaybackDev::IsDeviceSupportFmt(int device, int channel, int samplerate)
{
    int DevIdx;
    PaStreamParameters playParam;

    if (device >= 0)
    {
        DevIdx = device;
    }
    else
    {
        DevIdx = Pa_GetDefaultOutputDevice();
    }

    if (device < 0)
    {
        return 0;
    }

    playParam.device = device;
    playParam.channelCount = channel;
    playParam.sampleFormat = paInt16;
    playParam.hostApiSpecificStreamInfo = NULL;

    if (Pa_IsFormatSupported(&playParam, NULL, samplerate) == paFormatIsSupported)
    {
        return 1;
    }

    return 0;
}

int PlaybackDev::Pause()
{
   m_IsPaused = 1;
   return 0;
}
int PlaybackDev::Resume()
{
   m_IsPaused = 0;
   return 0;
}
int PlaybackDev::Flush()
{
#ifdef AV_OS_WIN32
    EnterCriticalSection(&(m_lock));
#else
    pthread_mutex_lock(&(m_lock));
#endif
    m_AudioBufAvail = 0;
    
#ifdef AV_OS_WIN32
    LeaveCriticalSection(&(m_lock));
#else
    pthread_mutex_unlock(&(m_lock));
#endif
    return 0;
}

int PlaybackDev::pushPlayData(unsigned char * pData, int size)
{
    int len = size;

#ifdef AV_OS_WIN32
    EnterCriticalSection(&(m_lock));
#else
    pthread_mutex_lock(&(m_lock));
#endif
    if (len > m_AudioBufMax - m_AudioBufAvail)
    {
        len = m_AudioBufMax - m_AudioBufAvail;

        //OLog_Dbg("[%s:%d] over flow\n", __FUNCTION__, __LINE__);
    }

    memcpy(m_AudioBuf + m_AudioBufAvail, pData, len);

    m_AudioBufAvail += len;

#ifdef AV_OS_WIN32
    LeaveCriticalSection(&(m_lock));
#else
    pthread_mutex_unlock(&(m_lock));
#endif

    return 0;
}

int PlaybackDev::PushData(unsigned char * pData, int FrameCnt)
{
    //return 0;

   if (m_IsPaused)
   {
      return 0;
   }

   if (m_AudConvertCtx)
   {
      uint8_t *data_in[AV_NUM_DATA_POINTERS];
      uint8_t *data_out[AV_NUM_DATA_POINTERS];
      int outsamples;
      data_in[0] = (unsigned char*)pData;
      data_out[0] = m_TmpBuf;

      outsamples = swr_convert(m_AudConvertCtx, data_out, m_outSampleRate / 5, (const uint8_t **)data_in, FrameCnt);
      if (outsamples > 0)
      {
          pushPlayData(m_TmpBuf, outsamples * 2 * m_outChannel);
      }
   }
   else
   {
       pushPlayData(pData, FrameCnt*2*m_outChannel);
   }
   return 0;
}

