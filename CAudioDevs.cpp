#include "CAudioDevs.h"
#include "anLogs.h"
#include "anMisc.h"

PlaybackDev::PlaybackDev()
{
   m_devIdx = 0;
   m_audConvertCtx = NULL;
   m_TmpBuf = NULL;
   m_IsPaused = 0;
   m_outChannel = 2;
   m_outSampleRate = 44100;
   TaskLoop = 0;
   m_flushFlag = 0;
   
   memset(&m_Task, 0, sizeof(m_Task));

   m_audioBufWp = 0;
   m_audioBufRp = 0;
   
   m_Task = NULL;
   m_maxCacheInMs = 300;

   m_audioBufMax = (m_outSampleRate * m_outChannel * 2 * m_maxCacheInMs) / 1000;
   m_audioBufAvail = 0;
   m_audioBuf = (unsigned char*)malloc(m_audioBufMax);

   //m_fileDebug = fopen("111.pcm", "wb");
   m_fileDebug = NULL;
}

int PlaybackDev::openAudioDev(PaStream **stream)
{
    int err;
    const PaDeviceInfo *devinfo;
    PaStreamParameters playParam;
    PaStream *stream_new = NULL;

    *stream = NULL;

    playParam.device = m_devIdx;
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
        m_outSampleRate / 5, //250ms perframe
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
void PlaybackDev::ThreadPlayback(void* p)
{
    PlaybackDev* pobjs = (PlaybackDev *)p;
    PaStream *stream = NULL;
    unsigned char * writeTmp; //48K也有100ms，足够了
    int read_size, read_expect;
    int avail_frame;
    int min_write_frme;

    //我们一次写入50ms，确保延迟
    min_write_frme = pobjs->m_outSampleRate / 20;
    writeTmp = (unsigned char*)calloc(1, pobjs->m_audioBufMax);

    while (pobjs->TaskLoop && writeTmp)
    {
        if (stream == NULL)
        {
            if (pobjs->openAudioDev(&stream) == 0)
            {
                avail_frame = Pa_GetStreamWriteAvailable(stream);
                if (avail_frame > 0)
                {
                    //打开成功，需要情况缓冲区
                    memset(writeTmp, 0, pobjs->m_audioBufMax);
                    Pa_WriteStream(stream, writeTmp, avail_frame / (2 * pobjs->m_outChannel));                    
                    continue;
                }
            }
            uv_sleep(20);
            continue;
        }

        if (pobjs->m_flushFlag == 1)
        {
            pobjs->readPlayData(writeTmp, pobjs->m_audioBufMax);
            pobjs->m_flushFlag = 0;
        }
        avail_frame = Pa_GetStreamWriteAvailable(stream);
        if (avail_frame >= min_write_frme)
        {
            read_expect = min_write_frme * 2 * pobjs->m_outChannel;
            
            //可以写入数据了，管他妈的，写
            read_size = pobjs->readPlayData(writeTmp, read_expect);
            if (read_size > 0)
            {
                //MessageOutput("read_size=%d, avail_frame=%d, buf_max=%d, avail=%d\n", read_size, read_expect, pobjs->m_audioBufMax, pobjs->m_audioBufAvail);
                
                if (pobjs->m_fileDebug)
                {
                    fwrite(writeTmp, 1, read_size, pobjs->m_fileDebug);
                }
                
                Pa_WriteStream(stream, writeTmp, read_size / (2 * pobjs->m_outChannel));
                continue;
            }
        }
        
        uv_sleep(20);
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
    
    return;
}

PlaybackDev::~PlaybackDev()
{
    PlayStop();
    if (m_fileDebug)
    {
        fclose(m_fileDebug);
        m_fileDebug = NULL;
    }
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
    if (m_audioBuf != NULL)
    {
        free(m_audioBuf);
        m_audioBuf = NULL;
    }

    m_audioBufMax = (m_outSampleRate * m_outChannel * 2 * m_maxCacheInMs) / 1000;
    m_audioBuf = (unsigned char*)malloc(m_audioBufMax);

}
int PlaybackDev::PlayStop()
{
    if (TaskLoop)
    {
        TaskLoop = 0;
        uv_thread_join(&m_Task);
    }
    memset(&m_Task, 0, sizeof(m_Task));
    
   if (m_audConvertCtx)
   {
      swr_free(&m_audConvertCtx);
      m_audConvertCtx = NULL;
   }

   if (m_TmpBuf)
   {
      free(m_TmpBuf);
      m_TmpBuf = NULL;
   }

   m_audioBufWp = 0;
   m_audioBufRp = 0;
   m_audioBufAvail = 0;

   return 0;
}

int PlaybackDev::PlayStart(int DiviceIdx, int channels, int samplerate)
{
   int layout;
   int layoutDst;

   PlayStop();

   m_IsPaused = 0;
   m_audioBufWp = 0;
   m_audioBufRp = 0;
   m_audioBufAvail = 0;
   m_channels = channels;
   m_samplerate = samplerate;
 
   if (DiviceIdx >= 0)
   {
      m_devIdx = DiviceIdx;
   }
	else
   {
      m_devIdx = Pa_GetDefaultOutputDevice();
   }

   if (m_devIdx < 0)
   {
      return -1;
   }

   if (IsDeviceSupportFmt(m_devIdx, m_outChannel, m_outSampleRate) == 0)
   {
       //如果不支持，我们就用默认的格式播放
       const PaDeviceInfo* devinfo = Pa_GetDeviceInfo(m_devIdx);
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
   m_TmpBuf = (unsigned char*)malloc(m_outSampleRate*m_outChannel*2);

   //should redo here
   if ((m_samplerate != m_outSampleRate) || (m_outChannel != m_channels))
   {
      m_audConvertCtx = swr_alloc_set_opts(m_audConvertCtx,
         layoutDst, AV_SAMPLE_FMT_S16, m_outSampleRate, //dst
         layout, AV_SAMPLE_FMT_S16, m_samplerate, //src 
         0, NULL);
      swr_init(m_audConvertCtx);
   }
   else
   {
      m_audConvertCtx = NULL;
   }

   TaskLoop = 1;
   if (uv_thread_create(&m_Task, PlaybackDev::ThreadPlayback, this) != 0)
   {
       memset(&m_Task, 0, sizeof(m_Task));
       TaskLoop = 0;
   }

   return 0;
}

int PlaybackDev::GetDeviceFmt(int device, int &channel, int &samplerate)
{
    channel = 2;
    samplerate = 4800;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }

    channel = 2;
    samplerate = 44100;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }

    channel = 2;
    samplerate = 32000;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }

    channel = 2;
    samplerate = 16000;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }

    channel = 1;
    samplerate = 4800;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }

    channel = 1;
    samplerate = 44100;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }
    
    channel = 1;
    samplerate = 32000;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }

    channel = 1;
    samplerate = 16000;
    if (IsDeviceSupportFmt(device, channel, samplerate) == 1)
    {
        return 1;
    }
    
    channel = 2;
    samplerate = 48000;
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
    m_flushFlag = 1;
    return 0;
}
int PlaybackDev::readPlayData(unsigned char * pData, int size)
{
    long availSize;

    //return 0;
    
    //自己实现一个无锁队列
    anmisc_data_exchange_long(&availSize, m_audioBufAvail);

    if (availSize < size)
    {
        return 0;
    }

    
    if (size > availSize)
    {
        size  = availSize;
    }

    //MessageWarn("[%s:%d]==> m_audioBufRp=%d, len=%d\n", __FUNCTION__, __LINE__, m_audioBufRp, size);
    
    if (m_audioBufRp + size <= m_audioBufMax)
    {
        memcpy(pData, m_audioBuf + m_audioBufRp, size);
        m_audioBufRp += size;
        if (m_audioBufRp >= m_audioBufMax)
        {
            m_audioBufRp = 0;
        }
    }
    else
    {
        int part_size = m_audioBufMax - m_audioBufRp;
        
        memcpy(pData, m_audioBuf + m_audioBufRp, part_size);
        memcpy(pData + part_size, m_audioBuf, size - part_size);
        
        m_audioBufRp = size - part_size; //更新新的位置
    }

    anmisc_data_exchange_add_long(&m_audioBufAvail, -size);
    
    return (int)size;
}

int PlaybackDev::pushPlayData(unsigned char * pData, int size)
{
    int len = size;
    long availSize;

    //自己实现一个无锁队列
    anmisc_data_exchange_long(&availSize, m_audioBufAvail);
    if (len > m_audioBufMax - availSize)
    {
        len = m_audioBufMax - availSize;
        MessageWarn("[%s:%d] over flow\n", __FUNCTION__, __LINE__);
        if (len <= 0)
        {
            return 0;
        }
    }

    
    //MessageWarn("[%s:%d] m_audioBufWp=%d, len=%d\n", __FUNCTION__, __LINE__, m_audioBufWp, len);
    
    //开始复制数据
    if (len + m_audioBufWp <= m_audioBufMax)
    {
        memcpy(m_audioBuf + m_audioBufWp, pData, len);
        m_audioBufWp += len;
    }
    else
    {
        int part_size = m_audioBufMax - m_audioBufWp;
        
        memcpy(m_audioBuf + m_audioBufWp, pData, part_size);
        memcpy(m_audioBuf, pData + part_size, len - part_size);
        
        m_audioBufWp = len - part_size; //更新新的位置
    }

    anmisc_data_exchange_add_long(&m_audioBufAvail, len);
    
    return 0;
}

int PlaybackDev::PushData(unsigned char * pData, int FrameCnt)
{
   if (m_IsPaused)
   {
      return 0;
   }
   
   if (m_audConvertCtx)
   {
      uint8_t *data_in[AV_NUM_DATA_POINTERS];
      uint8_t *data_out[AV_NUM_DATA_POINTERS];
      int outsamples;
      data_in[0] = (unsigned char*)pData;
      data_out[0] = m_TmpBuf;

      outsamples = swr_convert(m_audConvertCtx, data_out, m_outSampleRate, (const uint8_t **)data_in, FrameCnt);
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

