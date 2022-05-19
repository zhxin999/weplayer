#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <qglobal.h>
#include "anLogs.h"
#include "anMisc.h"
#include "anPlayer.h"
#include "anRingQueneLF.h"
#include "anListObject.h"

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#endif


#define FILE_NAME_MAX       2048
#define AVPACKET_CACHED_MAX     512

#define ABS_TICKS(x, y) ((x)>(y)?((x)-(y)):((y)-(x)))

enum
{
  ANFILTER_POS_SUBTEXT = 0,
  ANFILTER_POS_MAX
};

enum
{
   CAVQUE_CMD_RESET = 100,
   CAVQUE_CMD_MAX
};

typedef enum _TIMER_CTR_CMD
{
   TIMER_CMD_NONE = 0,
   TIMER_CMD_QUIT,
   TIMER_CMD_PAUSE,
   TIMER_CMD_START,
   TIMER_CMD_SETBASE,
   TIMER_CMD_SPEED

}TIMER_CTR_CMD_t;

typedef enum _READER_CMD
{
   READER_CMD_Reopen = 0,
   READER_CMD_Pause,
   READER_CMD_Play,
   READER_CMD_Seek,
}READER_CMD_t;

typedef enum _INFILE_TYPE
{
   INSTREAM_TYPE_FILE,
   INSTREAM_TYPE_NET_STREAM,
   INSTREAM_TYPE_DSHOW
}INFILE_TYPE_t;

typedef enum _TICKS_AXIS_UPDATE
{
   TICKS_AXIS_BY_VIDEO = 0,
   TICKS_AXIS_BY_AUDIO,
   TICKS_AXIS_BY_TIMER

}TICKS_AXIS_UPDATE_t;


typedef struct _AvCmdList_
{
	OBJ_BASE_MEMBER;
	
	int Code;
	long Param1;
	void* Param2;
	int64_t Param3;
}CMDList_t;

typedef struct _AvFrameList_
{
	OBJ_BASE_MEMBER;
	
	AVFrame* frame;
}AvrFrameList_t;

typedef struct _FILTER_DESC__
{
	char szFilterDesc[1024];

	int bUsed;
	
   AVFilterContext* buffersink_ctx;
   AVFilterContext* buffersrc_ctx;
   AVFilterGraph*   filter_graph;
}FILTER_DATA;


struct _ANPlayerData
{
   uv_thread_t taskHandleReader;
   uv_thread_t taskHandleVideo;
   uv_thread_t taskHandleAudio;
   uv_thread_t taskHandleTimer;
   int task_loop;
   int task_loop_reader;
   int task_loop_video;
   int task_loop_audio;

   char szFilename[FILE_NAME_MAX];
   char szSubtextFilter[FILE_NAME_MAX];
   
   void* UserData;

   ANPlayer_Video_CB  VideoCb;
   ANPlayer_Audio_CB  AudioCb;
   ANPlayer_Event_CB  EventCb;

   //是否是否指定个数输出,如果不指定，就设置m_EventID;AV_PIX_FMT_NONE
   AVPixelFormat VideoOutFmt;
   int      VideoOutWidth;
   int      VideoOutHeight;

   //设置输出的声音格式, 如果m_audio_channels = 0， 表示不用转换直接输出
   uint16_t AudioOutSampleRate;
   uint16_t AudioOutChannels;

   //正常值100
   int  PlaySpeed;

   //播放器状态
   ANPLAYER_STATE_CODE state;

   //命令队列,用来相互之间通信的
   CMDList_t* pReaderCmdList;
   CMDList_t* pTimerCmdList;
   CMDList_t* pVideoDecCmdList;
   CMDList_t* pAudioDecCmdList;

   //未解码的视频音频队列
   ANRingQueneLF_h VideoPktQ;
   ANRingQueneLF_h AudioPktQ;

   //互斥锁
   uv_mutex_t lock;

   //播放相关的数据
   AVFormatContext *fmt_ctx;
   int quit_timeout;
   int64_t quit_time_base;

   INFILE_TYPE_t instream_type;
   int video_stream_idx;
   int audio_stream_idx;

   int64_t time_axis_play;
   TICKS_AXIS_UPDATE_t time_axis_type;

   int reach_to_end;
};

typedef struct _ANPlayerData ANPlayer_t;


int64_t anPlayer_Get_SysTime_ms()
{
#ifdef _WIN32
   return timeGetTime();
#else
   return av_gettime_relative() / 1000;
    //return av_gettime() / 1000;
#endif
}

int GetNextNalu(unsigned char *pH264Data, int DataSize, int offset)
{
   int i;

   for (i = offset; i < DataSize - 4; i++)
   {
      if ((pH264Data[i + 0] == 0) && (pH264Data[i + 1] == 0)
         && (pH264Data[i + 2] == 0) && (pH264Data[i + 3] == 1))
      {
         return i;
      }

      if ((pH264Data[i + 0] == 0) && (pH264Data[i + 1] == 0)
         && (pH264Data[i + 2] == 0x01))
      {
         return i;
      }
   }

   return 0;
}


int RebuildH264ExtraInfo(unsigned char* pH264Data, int H264DataLen, unsigned char* extradata)
{
   int i;
   int ret = 0;
   int ppsStart, ppsSize;
   int spsStart, spsSize;
   int FindStart, FindSize;
   int startcode = 0;

   ppsSize = -1;
   spsSize = -1;
   ret = 0;

   for (i = 0; i < H264DataLen - 4; i++)
   {
      startcode = 0;

      if ((pH264Data[i + 0] == 0) && (pH264Data[i + 1] == 0)
         && (pH264Data[i + 2] == 0) && (pH264Data[i + 3] == 1))
         startcode = 4;

      if ((pH264Data[i + 0] == 0) && (pH264Data[i + 1] == 0)
         && (pH264Data[i + 2] == 1))
         startcode = 3;


      if (startcode > 0)
      {
         unsigned char type = pH264Data[i + startcode] & 0x01F;
         int nextNalu = GetNextNalu(pH264Data, H264DataLen, i + startcode);

         FindStart = i;

         if (nextNalu > i + startcode)
         {
            FindSize = nextNalu - i;
         }
         else
         {
            FindSize = H264DataLen - i;
         }

         if (type == 7)//sps
         {
            spsStart = FindStart;
            spsSize = FindSize;
            
         }
         else if (type == 8)//pps
         {
            //printf("find pps, size[%d - %d]\n", FindStart, FindSize);
            ppsStart = FindStart;
            ppsSize = FindSize;

            //m_PPsLen = ppsSize;
            //memcpy(m_szPPs, pH264Data + ppsStart, m_PPsLen);
         }

         ret = type;

         i += 3;
      }
   }

   if ((ppsSize > 0) && (spsSize > 0) && extradata)
   {
      ret = ppsSize + spsSize;

      memcpy(extradata, pH264Data + spsStart, spsSize);
      memcpy(extradata + spsSize, pH264Data + ppsStart, ppsSize);

      //printf("c->extradata_size = %d\n", c->extradata_size);
   }
   return ret;
}

int anPlayer_Subtext_VFilter_Create(ANPlayer_t* pPlayer, AVStream *stream_video,FILTER_DATA* Filters_Subtext)
{
   int ret = 0;
   char args[512];
   const AVFilter*buffersrc = NULL;
   const AVFilter*buffersink = NULL;
   AVFilterContext*buffersrc_ctx = NULL;
   AVFilterContext*buffersink_ctx = NULL;
   AVFilterInOut*outputs = avfilter_inout_alloc();
   AVFilterInOut*inputs = avfilter_inout_alloc();
   AVFilterGraph*filter_graph = avfilter_graph_alloc();
   const char *filter_spec = pPlayer->szSubtextFilter;

   MessageOutput("[%s:%d] try to use filter[%s] \n", __FUNCTION__, __LINE__, filter_spec);

   if (!outputs || !inputs || !filter_graph)
   {
      ret = -1;
      MessageError("[%s:%d] no memory \n", __FUNCTION__, __LINE__);
      goto end;
   }

   buffersrc = avfilter_get_by_name("buffer");
   buffersink = avfilter_get_by_name("buffersink");
   if (!buffersrc || !buffersink)
   {
      ret = -1;
      MessageError("[%s:%d] filteringsource or sink element not found \n", __FUNCTION__, __LINE__);
      goto end;
   }

   //_snprintf(args, sizeof(args), "filename=E\\:\\\\VideoFiles\\\\subtitle.ass");

   snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
      stream_video->codecpar->width, stream_video->codecpar->height, stream_video->codecpar->format,
      stream_video->time_base.num, stream_video->time_base.den,
      stream_video->sample_aspect_ratio.num,
      stream_video->sample_aspect_ratio.den);

   ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
   if (ret < 0)
   {
      ret = -1;
      MessageError("[%s:%d] Cannot create buffer source\n", __FUNCTION__, __LINE__);
      goto end;
   }
   ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
   if (ret < 0)
   {
      ret = -1;
      MessageError("[%s:%d] Cannot create buffer sink\n", __FUNCTION__, __LINE__);
      goto end;
   }

   /* Endpoints for the filter graph. */
   outputs->name = av_strdup("in");
   outputs->filter_ctx = buffersrc_ctx;
   outputs->pad_idx = 0;
   outputs->next = NULL;
   inputs->name = av_strdup("out");
   inputs->filter_ctx = buffersink_ctx;
   inputs->pad_idx = 0;
   inputs->next = NULL;
   if (!outputs->name || !inputs->name)
   {
      ret = -1;
      MessageError("[%s:%d] no memory \n", __FUNCTION__, __LINE__);
      goto end;
   }
   //"subtitles=filename=E\\\\:/VideoFiles/subtitle.srt"
   //if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs, &outputs, NULL)) < 0)
   if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs, &outputs, NULL)) < 0)
   {
      ret = -1;
      MessageError("[%s:%d] parser filter spec failed[%s] \n", __FUNCTION__, __LINE__, filter_spec);

      if (pPlayer->EventCb)
      {
         pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_SUBTEXT_FAILED, 0, NULL, NULL, pPlayer->UserData);
      }

      goto end;
   }
   if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
   {
      ret = -1;
      MessageError("[%s:%d] filter config failed \n", __FUNCTION__, __LINE__);
      goto end;
   }

   Filters_Subtext->buffersrc_ctx = buffersrc_ctx;
   Filters_Subtext->buffersink_ctx = buffersink_ctx;
   Filters_Subtext->filter_graph = filter_graph;
   Filters_Subtext->bUsed = 1;
   
end:
   if (ret < 0)
   {
      avfilter_graph_free(&filter_graph);
   }
   avfilter_inout_free(&inputs);
   avfilter_inout_free(&outputs);
   return ret;
}

static int anPlayer_Flush_AvPacketList(ANRingQueneLF_h AvPacketList, uint64_t BreakOnSpecial)
{
    AVPacket* pkt;
    int loops = 1000;
    uint64_t pkt_special;

    while (ANRingQLF_Get_Count(AvPacketList) != 0)
    {
        pkt = (AVPacket* )ANRingQLF_Header_Get(AvPacketList);
        if (pkt)
        {
            pkt_special = (uint64_t)pkt;
            if (pkt_special > 100) //内存地址可能有这种
            {
                av_packet_unref(pkt);
                ANRingQLF_Header_Pop(AvPacketList);
            }
            else
            {
                ANRingQLF_Header_Pop(AvPacketList);
                if (pkt_special == BreakOnSpecial)
                {
                    break;
                }
            }
        }
        else
        {
            av_usleep(1000);
        }

        //防止死循环,不可能循环100次
        loops--;
        if (loops <= 0)
        {
            break;
        }
    }

    return 0;
}
static int anPlayer_Push_AvPacketList(ANRingQueneLF_h AvPacketList, AVPacket* pkt)
{
   AVPacket* pkt_free;
   int loops = 1000;
   int ret = -1;
   uint64_t pkt_special;

   pkt_special = (uint64_t)pkt;
   
   while (loops >= 0)
   {
      pkt_free = (AVPacket* )ANRingQLF_Back_Get(AvPacketList);
      if (pkt_free)
      {
         if (pkt_special > 100)
         {
             av_init_packet(pkt_free);
             av_packet_ref(pkt_free, pkt);
         }
         else
         {
            pkt_free = pkt;
         }
         ANRingQLF_Back_Push(AvPacketList);
         ret = 0;
         break;
      }

      //防止死循环,不可能循环100次
      loops--;
   }

   return ret;
}
static int anPlayer_Push_AvFrameList(ANRingQueneLF_h AvFrameList, AVFrame* frm)
{
   AVFrame** frm_free;
   int ret = -1;
   
   frm_free = (AVFrame** )ANRingQLF_Back_Get(AvFrameList);
   if (frm_free)
   {
      *frm_free = frm;
      ANRingQLF_Back_Push(AvFrameList);
      ret = 0;
   }

   return ret;
}

static AVFrame* anPlayer_Pop_AvFrameList(ANRingQueneLF_h AvFrameList)
{
   AVFrame** frm_free;
   AVFrame* frm = NULL;

   frm_free = (AVFrame** )ANRingQLF_Header_Get(AvFrameList);
   if (frm_free)
   {
      frm = *frm_free;
      *frm_free = NULL;
      ANRingQLF_Header_Pop(AvFrameList);
   }

   return frm;
}

static int anPlayer_Flush_AvFrameList(ANRingQueneLF_h AvFrameList)
{
    AVFrame* frm = NULL;

    while (ANRingQLF_Get_Count(AvFrameList) != 0)
    {
        frm = anPlayer_Pop_AvFrameList(AvFrameList);
        if (frm)
        {
           av_frame_free(&frm);
        }
        else
        {
            break;
        }
    }

    return 0;
}

int anPlayer_Play_Seek(ANPlayer_t* pPlayer, int64_t new_pos)
{
   int64_t new_seek_pos = new_pos;
   int err;
   int is_seeked = 0;

   if (pPlayer->video_stream_idx >= 0)
   {
       AVStream *videoStream = pPlayer->fmt_ctx->streams[pPlayer->video_stream_idx];

       int64_t pos_in_timeunit = ((videoStream->time_base.den * new_seek_pos)/(videoStream->time_base.num * 1000));

       if (videoStream->start_time != AV_NOPTS_VALUE)
       {
          pos_in_timeunit += videoStream->start_time;
       }

       err = avformat_seek_file(pPlayer->fmt_ctx, pPlayer->video_stream_idx, INT64_MIN, pos_in_timeunit , INT64_MAX, AVSEEK_FLAG_BACKWARD);//AVSEEK_FLAG_BACKWARD
       if (err >= 0)
       {
          is_seeked = 1;
          new_seek_pos *= 1000;
       }
   }

   if (is_seeked == 0)
   {
       new_seek_pos *= 1000;

       if (pPlayer->fmt_ctx->start_time != AV_NOPTS_VALUE)
       {
          err = avformat_seek_file(pPlayer->fmt_ctx, -1, INT64_MIN, new_seek_pos + pPlayer->fmt_ctx->start_time, INT64_MAX, AVSEEK_FLAG_BACKWARD);//AVSEEK_FLAG_BACKWARD
       }
       else
       {
          err = avformat_seek_file(pPlayer->fmt_ctx, -1, INT64_MIN, new_seek_pos , INT64_MAX, AVSEEK_FLAG_BACKWARD);//AVSEEK_FLAG_BACKWARD
       }
   }

   if (err >= 0)
   {
      MessageOutput("[%s:%d] seek ok [%d]s\n", __FUNCTION__, __LINE__, new_pos / 1000);
   }
   else
   {
      MessageError("[%s:%d] seek failed:%d\n", __FUNCTION__, __LINE__, err);
      return -1;
   }

   //清空缓存队列
   //anPlayer_Flush_AvPacketList(pPlayer->AudioPktQ);
   //anPlayer_Flush_AvPacketList(pPlayer->VideoPktQ);
   
   //应该把视频和音频线程的缓存的数据给清空，要不然因为pts的原因可能会卡死
   CMDList_t *Msg = (CMDList_t *)malloc(sizeof(CMDList_t));
   if (Msg)
   {
      Msg->Code = TIMER_CMD_START;
      Msg->Param3 = new_seek_pos/1000;
      
      uv_mutex_lock(&(pPlayer->lock));      
      ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pTimerCmdList), (OBJ_BASE_t*)Msg);
      uv_mutex_unlock(&(pPlayer->lock));
   }

   if (pPlayer->video_stream_idx >= 0)
   {
       anPlayer_Push_AvPacketList(pPlayer->VideoPktQ, (AVPacket*)1);
       
       Msg = (CMDList_t *)malloc(sizeof(CMDList_t));
       if (Msg)
       {
          Msg->Code = CAVQUE_CMD_RESET;
          uv_mutex_lock(&(pPlayer->lock));      
          ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pVideoDecCmdList), (OBJ_BASE_t*)Msg);
          uv_mutex_unlock(&(pPlayer->lock));
       }
   }

   if (pPlayer->audio_stream_idx >= 0)
   {
       anPlayer_Push_AvPacketList(pPlayer->AudioPktQ, (AVPacket*)1);
       
       Msg = (CMDList_t *)malloc(sizeof(CMDList_t));
       if (Msg)
       {
          Msg->Code = CAVQUE_CMD_RESET;
          uv_mutex_lock(&(pPlayer->lock));      
          ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pAudioDecCmdList), (OBJ_BASE_t*)Msg);
          uv_mutex_unlock(&(pPlayer->lock));
       }
   }
   
   if (pPlayer->EventCb)
   {
      pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_SEEK_DONE, 0, NULL, NULL, pPlayer->UserData);
   }

   return 0;
}


void AvFrame_List_Release_Cb(OBJ_BASE_t* pNode)
{
   AvrFrameList_t * frmList = (AvrFrameList_t*)pNode;
   if (frmList)
   {
      av_frame_free(&frmList->frame);
   }
}

static int anPlayer_Openfile_callback(void* arg)
{
   ANPlayer_t* pPlayer = (ANPlayer_t *)arg;

   if (pPlayer->task_loop == 0)
      return 1;

   if (av_gettime_relative() - pPlayer->quit_time_base  > pPlayer->quit_timeout)
   {
      return 1;
   }
   
   return 0;
}

static int anPlayer_Open_CodecCtx(int *stream_idx,	AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret, stream_index;
	AVStream *st;
	AVCodec *dec = NULL;
	AVDictionary *opts = NULL;
   char szErr[256];

	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) 
	{
		return ret;
	}
	else
	{
		stream_index = ret;
		st = fmt_ctx->streams[stream_index];

		/* find decoder for the stream */
		dec = avcodec_find_decoder(st->codecpar->codec_id);
		if (!dec) 
		{
			MessageError("[%s:%d]Failed to find codec [%s]\n", __FUNCTION__, __LINE__, av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Allocate a codec context for the decoder */
		*dec_ctx = avcodec_alloc_context3(dec);
		if (!*dec_ctx) 
		{
			MessageError("[%s:%d]Failed to allocate the %s codec contex\n", __FUNCTION__, __LINE__, av_get_media_type_string(type));
			return AVERROR(ENOMEM);
		}

		/* Copy codec parameters from input stream to output codec context */
		if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0)
		{
			MessageError("[%s:%d]Failed to copy %s codec parameters to decoder context\n", __FUNCTION__, __LINE__, av_get_media_type_string(type));
			return ret;
		}

		/* Init the decoders, with or without reference counting */
		av_dict_set(&opts, "refcounted_frames", "1", 0);
		if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) 
		{
         av_make_error_string(szErr, sizeof(szErr), ret);
			MessageError("[%s:%d]Failed to open %s codec, %s\n", __FUNCTION__, __LINE__, av_get_media_type_string(type), szErr);
			return ret;
		}

		*stream_idx = stream_index;
	}

	return 0;
}

static int anPlayer_Close_File(ANPlayer_t* pPlayer)
{
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Param\n", __FUNCTION__, __LINE__);
      return -1;
   }

   if (pPlayer->task_loop_audio)
   {
       pPlayer->task_loop_video = 0;
       uv_thread_join(&(pPlayer->taskHandleVideo));
   }

   if (pPlayer->task_loop_video)
   {
       pPlayer->task_loop_audio = 0;
       uv_thread_join(&(pPlayer->taskHandleAudio));
   }

   memset(&pPlayer->taskHandleVideo, 0, sizeof(pPlayer->taskHandleVideo));
   memset(&pPlayer->taskHandleAudio, 0, sizeof(pPlayer->taskHandleAudio));

   if (pPlayer->fmt_ctx)
   {
      avformat_close_input(&pPlayer->fmt_ctx);
      pPlayer->fmt_ctx = NULL;
   }

   uv_mutex_lock(&(pPlayer->lock));    

   //清空队列
   if (pPlayer->pAudioDecCmdList)
   {
      //暂时不考虑清空自定义数据，还没想好有哪些自定义数据
      ObjBase_FreeList_UserData((OBJ_BASE_t *)pPlayer->pAudioDecCmdList, NULL);
      pPlayer->pAudioDecCmdList = NULL;
   }

   if (pPlayer->pVideoDecCmdList)
   {
      //暂时不考虑清空自定义数据，还没想好有哪些自定义数据
      ObjBase_FreeList_UserData((OBJ_BASE_t *)pPlayer->pVideoDecCmdList, NULL);
      pPlayer->pVideoDecCmdList = NULL;
   }

   if (pPlayer->pTimerCmdList)
   {
      //暂时不考虑清空自定义数据，还没想好有哪些自定义数据
      ObjBase_FreeList_UserData((OBJ_BASE_t *)pPlayer->pTimerCmdList, NULL);
      pPlayer->pTimerCmdList = NULL;
   }

   uv_mutex_unlock(&(pPlayer->lock));
   return 0;
}

static int anPlayer_Open_File(ANPlayer_t* pPlayer)
{
    char* FileName;
    AVInputFormat *iformat = NULL;
    int err;
    AVDictionary* options = NULL;
    char szError[256];

    FileName = pPlayer->szFilename;

    MessageOutput("[%s:%d]try to open[%s]\n", __FUNCTION__, __LINE__, FileName);

    pPlayer->fmt_ctx = avformat_alloc_context();
    pPlayer->quit_time_base = av_gettime_relative();
    pPlayer->quit_timeout = 60000000;
    pPlayer->fmt_ctx->interrupt_callback.callback = anPlayer_Openfile_callback;
    pPlayer->fmt_ctx->interrupt_callback.opaque = pPlayer;

    pPlayer->instream_type = INSTREAM_TYPE_FILE;
    if (strncmp(FileName,"udp://", strlen("udp://")) == 0)
    {
        pPlayer->fmt_ctx->max_analyze_duration = 60000000;
        pPlayer->fmt_ctx->format_probesize = 50000000;
        pPlayer->instream_type = INSTREAM_TYPE_NET_STREAM;
        iformat = av_find_input_format("mpegts");
    }
    else if (strncmp(FileName,"rtsp://", strlen("rtsp://")) == 0)
    {
        pPlayer->fmt_ctx->max_analyze_duration = 60000000;
        pPlayer->fmt_ctx->format_probesize = 50000000;
        pPlayer->instream_type = INSTREAM_TYPE_NET_STREAM;

        //看看是不是支持tcp
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
    }

    err = avformat_open_input(&pPlayer->fmt_ctx, FileName, iformat, &options);  //&options
    if (err < 0)
    {
        av_strerror(err, szError, sizeof(szError));
        MessageError("[%s:%d]Could not open source file [%s], error[%s]\n", __FUNCTION__, __LINE__, FileName, szError);
        anPlayer_Close_File(pPlayer);
        av_dict_free(&options);
        return -1;
    }

    if (avformat_find_stream_info(pPlayer->fmt_ctx, NULL) < 0)
    {
        MessageError("[%s:%d]Could not find stream information [%s]\n", __FUNCTION__, __LINE__, FileName);
        av_dict_free(&options);
        anPlayer_Close_File(pPlayer);
        return -1;
    }
    av_dict_free(&options);

    err = av_find_best_stream(pPlayer->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (err >= 0)
    {
        pPlayer->video_stream_idx = err;
        if ((pPlayer->fmt_ctx->streams[pPlayer->video_stream_idx]->codecpar->width == 0) || 
            (pPlayer->fmt_ctx->streams[pPlayer->video_stream_idx]->codecpar->height == 0))
        {
            MessageError("[%s:%d]Could not find stream information [%s]\n", __FUNCTION__, __LINE__, FileName);
            anPlayer_Close_File(pPlayer);
            return -1;
        }
    }
    else
    {
        pPlayer->video_stream_idx = -1;
    }

    err = av_find_best_stream(pPlayer->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (err >= 0)
    {
        pPlayer->audio_stream_idx = err;
    }
    else
    {
        pPlayer->audio_stream_idx = -1;
    }

    if (pPlayer->EventCb)
    {
        pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_OPEN_DONE, 0, NULL, pPlayer->fmt_ctx, pPlayer->UserData);
    }

	MessageOutput("[%s:%d] open done[%s]\n", __FUNCTION__, __LINE__,FileName);
    return 0;
}

void anPlayer_Thread_Video_dec(void* param)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)param;
   AVStream *video_stream;
   AVCodecContext *video_dec_ctx;
   AVFrame* video_frame_conveterd;
   struct SwsContext *video_convert_ctx;
   int ret;
   AVPacket * new_pkt;
   AVFrame* frame_ready;
   int64_t frame_ready_pts_ms;
   int64_t playtime_ms;   
   int64_t frame_pre_disp;
   //基准时间
   int64_t video_start_time_ms = AV_NOPTS_VALUE;
   AVCodec *dec;
   AVDictionary *opts;
   char szErr[256];
   ANRingQueneLF_h pVideoFrameList;
   //播放开始的时间，用来记录时间戳
   int64_t play_start_ms = AV_NOPTS_VALUE;
   int64_t play_start_stream = AV_NOPTS_VALUE;
   int64_t system_time_ms;
   //filter描述
   FILTER_DATA Filters_Subtext;
   int empty_loop = 0;
   int need_new_key = 0;

   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Param\n", __FUNCTION__, __LINE__);
      return;
   }

   memset(&Filters_Subtext, 0, sizeof(Filters_Subtext));
   opts = NULL;
   video_stream = NULL;
   video_dec_ctx = NULL;
   video_frame_conveterd = NULL;
   video_convert_ctx = NULL;
   frame_ready = NULL;

   pVideoFrameList = ANRingQLF_Create(64, sizeof(AVFrame*));
   if (pVideoFrameList == NULL)
   {
      MessageError("[%s:%d]Failed to create ring buffer\n", __FUNCTION__, __LINE__);
      goto quit;
   }

   video_stream = pPlayer->fmt_ctx->streams[pPlayer->video_stream_idx];

   dec = NULL;
   /*if (video_stream->codecpar->codec_id == AV_CODEC_ID_H264)
   {
        dec = avcodec_find_decoder_by_name("h264_vda");
   }*/

   if (dec == NULL)
   {
       dec = avcodec_find_decoder(video_stream->codecpar->codec_id);
       if (!dec)
       {
          MessageError("[%s:%d]Failed to find codec [%s]\n", __FUNCTION__, __LINE__, avcodec_get_name(video_stream->codecpar->codec_id));
          goto quit;
       }
   }

   video_dec_ctx = avcodec_alloc_context3(dec);
   if (!video_dec_ctx)
   {
      MessageError("[%s:%d]Failed to allocate the %s codec contex\n", __FUNCTION__, __LINE__, avcodec_get_name(video_stream->codecpar->codec_id));
      goto quit;
   }

   if ((ret = avcodec_parameters_to_context(video_dec_ctx, video_stream->codecpar)) < 0)
   {
      MessageError("[%s:%d]Failed to copy %s codec parameters to decoder context\n", __FUNCTION__, __LINE__,\
         avcodec_get_name(video_stream->codecpar->codec_id));
      goto quit;
   }

   //修改extradata,因为udp的夏洛特烦恼打不开,也许这是ffmpeg 3.2的一个bug，0x67的pps在前面，然后再找sps,否则解码器打开失败
   if ((video_dec_ctx->extradata_size > 0) && video_dec_ctx->extradata && video_dec_ctx->codec_id == AV_CODEC_ID_H264)
   {
      uint8_t* extra_data = (uint8_t*)av_mallocz(video_dec_ctx->extradata_size);

      if (extra_data)
      {
         if (RebuildH264ExtraInfo(video_dec_ctx->extradata, video_dec_ctx->extradata_size, extra_data) > 0)
         {
            memcpy(video_dec_ctx->extradata, extra_data, video_dec_ctx->extradata_size);
         }
         av_free(extra_data);
      }
   }

   //video_dec_ctx->thread_count = 4;
   //av_dict_set(&opts, "threads", "auto", 0);   
   av_dict_set(&opts, "threads", "auto", 0);
   //av_dict_set(&opts, "refcounted_frames", "1", 0);
   if ((ret = avcodec_open2(video_dec_ctx, dec, &opts)) < 0)
   {
      av_make_error_string(szErr, sizeof(szErr), ret);
      MessageError("[%s:%d]Failed to open %s codec, %s\n", __FUNCTION__, __LINE__, \
         avcodec_get_name(video_stream->codecpar->codec_id), szErr);
      goto quit;
   }

   video_start_time_ms = 0;
   if (video_stream->start_time != AV_NOPTS_VALUE)
   {
      video_start_time_ms = (int64_t)(video_stream->start_time * video_stream->time_base.num * 1000) / video_stream->time_base.den;
   }

   //
   play_start_stream = video_start_time_ms;
   play_start_ms = anPlayer_Get_SysTime_ms() + 500;
   frame_pre_disp = AV_NOPTS_VALUE;

   if (strlen(pPlayer->szSubtextFilter) > 0)
   {
       anPlayer_Subtext_VFilter_Create(pPlayer, video_stream, &Filters_Subtext);
   }

   need_new_key = 1;
   while ((pPlayer->task_loop == 1) && (pPlayer->task_loop_video == 1))
   {
      empty_loop = 0;
      
      if (pPlayer->pVideoDecCmdList)
      {
         CMDList_t* pNewCmd = NULL;
      
         uv_mutex_lock(&(pPlayer->lock));
         pNewCmd = (CMDList_t*)ObjBase_List_GetHead((OBJ_BASE_t**)&(pPlayer->pVideoDecCmdList));
         uv_mutex_unlock(&(pPlayer->lock));
         
         if (pNewCmd)
         {
            if (pNewCmd->Code == CAVQUE_CMD_RESET)
            {
               if (frame_ready)
               {
                  av_frame_free(&frame_ready);
                  frame_ready = NULL;
               }
               
               frame_pre_disp = AV_NOPTS_VALUE;
               need_new_key = 1;
               
               anPlayer_Flush_AvFrameList(pVideoFrameList);
               anPlayer_Flush_AvPacketList(pPlayer->VideoPktQ, 1);

               avcodec_flush_buffers(video_dec_ctx);
               MessageError("[%s:%d] reset video thread\n", __FUNCTION__, __LINE__);
            }

            free(pNewCmd);
         }
      }

      //更新时间戳
      if (pPlayer->time_axis_type == TICKS_AXIS_BY_VIDEO)
      {
         system_time_ms = anPlayer_Get_SysTime_ms();         
         anmisc_data_exchange_int64(&(pPlayer->time_axis_play), system_time_ms - play_start_ms + play_start_stream);
      }

      if (frame_ready == NULL)
      {
         frame_ready = anPlayer_Pop_AvFrameList(pVideoFrameList);

         if (frame_ready)
         {
            if (frame_ready->pts != AV_NOPTS_VALUE)
            {
                frame_ready_pts_ms = (frame_ready->pts * video_stream->time_base.num * 1000) / video_stream->time_base.den;
            }
            else
            {
                frame_ready_pts_ms = (frame_ready->pkt_dts * video_stream->time_base.num * 1000) / video_stream->time_base.den;
            }

            if (pPlayer->VideoOutFmt != AV_PIX_FMT_NONE)
            {
               if (video_convert_ctx == NULL)
               {
                  if (pPlayer->VideoOutWidth > 0)
                  {
                      if ((frame_ready->format != pPlayer->VideoOutFmt) 
                          || (frame_ready->width != pPlayer->VideoOutWidth) 
                          || (frame_ready->height != pPlayer->VideoOutHeight))
                      {
                          video_convert_ctx = sws_getContext(frame_ready->width, frame_ready->height, (AVPixelFormat)frame_ready->format, \
                             pPlayer->VideoOutWidth, pPlayer->VideoOutHeight, pPlayer->VideoOutFmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
                      }
                  }
                  else
                  {
                      if (frame_ready->format != pPlayer->VideoOutFmt)
                      {
                          video_convert_ctx = sws_getContext(frame_ready->width, frame_ready->height, (AVPixelFormat)frame_ready->format, \
                             frame_ready->width, frame_ready->height, pPlayer->VideoOutFmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
                      }
                  }
               }

               if ((video_frame_conveterd == NULL) && (video_convert_ctx))
               {
                  video_frame_conveterd = av_frame_alloc();

                  video_frame_conveterd->format = pPlayer->VideoOutFmt;
                  if (pPlayer->VideoOutWidth > 0)
                  {
                     video_frame_conveterd->width = pPlayer->VideoOutWidth;
                     video_frame_conveterd->height = pPlayer->VideoOutHeight;
                  }
                  else
                  {
                     video_frame_conveterd->width = frame_ready->width;
                     video_frame_conveterd->height = frame_ready->height;
                  }

                  av_frame_get_buffer(video_frame_conveterd, 32);
               }

               if (video_convert_ctx && video_frame_conveterd)
               {
                  sws_scale(video_convert_ctx, frame_ready->data, frame_ready->linesize, 0, frame_ready->height, \
                     video_frame_conveterd->data, video_frame_conveterd->linesize);
               }
            }


         }
      }

      if (frame_ready)
      {
         anmisc_data_exchange_int64(&playtime_ms, pPlayer->time_axis_play);
         if (frame_ready_pts_ms <= playtime_ms)
         {
            //渲染
            //如果是非常老的数据，直接丢掉
            if ((playtime_ms - frame_ready_pts_ms <= 1000) || (ABS_TICKS(frame_pre_disp, frame_ready_pts_ms) > 100))
            {
                frame_pre_disp = frame_ready_pts_ms;
               if (video_frame_conveterd != NULL)
                {                   
                   if (pPlayer->VideoCb)
                   {
                      pPlayer->VideoCb(pPlayer, video_frame_conveterd, frame_ready_pts_ms - video_start_time_ms, pPlayer->UserData);
                   }
                }
                else
                {
                   if (pPlayer->VideoCb)
                   {
                      pPlayer->VideoCb(pPlayer, frame_ready, frame_ready_pts_ms - video_start_time_ms, pPlayer->UserData);
                   }
                }
            }
            else
            {
               MessageOutput("[%s:%d] drop video...\n", __FUNCTION__, __LINE__);
            }

            //渲染完成，释放
            av_frame_free(&frame_ready);
            frame_ready = NULL;

         }
      }

      if (ANRingQLF_Get_Count(pVideoFrameList) <= 10)
      {
          uint64_t pkt_special;
         //decode....         
         new_pkt = (AVPacket* )ANRingQLF_Header_Get(pPlayer->VideoPktQ);
         pkt_special = (uint64_t)new_pkt;
         
         if (new_pkt && (pkt_special > 100))
         {            
            //MessageOutput("[%s:%d] push video packet\n", __FUNCTION__, __LINE__);
             AVFrame* frame_dec = NULL;
             int decode_loops = 0;

             if ((need_new_key == 1) && ((new_pkt->flags & AV_PKT_FLAG_KEY) == 0))
             {
                 av_packet_unref(new_pkt);
                 ANRingQLF_Header_Pop(pPlayer->VideoPktQ);
                 continue;
             }
             need_new_key = 0;
            avcodec_send_packet(video_dec_ctx, new_pkt);
            av_packet_unref(new_pkt);
            ANRingQLF_Header_Pop(pPlayer->VideoPktQ);       

            while (decode_loops++ < 20)
            {
                frame_dec = av_frame_alloc();
                if (frame_dec == NULL)
                {
                   MessageError("[%s:%d] Failed av_frame_alloc()\n", __FUNCTION__, __LINE__);
                   break;
                }
            
                ret = avcodec_receive_frame(video_dec_ctx, frame_dec);
                if (ret < 0)
                {
                    break;
                }
                
                if ((Filters_Subtext.bUsed && (Filters_Subtext.filter_graph)))
                {
                   AVFrame *frame_filter = av_frame_alloc();
                   if (frame_filter)
                   {
                      av_buffersrc_add_frame_flags(Filters_Subtext.buffersrc_ctx, frame_dec, AV_BUFFERSRC_FLAG_KEEP_REF);
                
                      int ret = av_buffersink_get_frame(Filters_Subtext.buffersink_ctx, frame_filter);
                      if (ret >= 0)
                      {
                          if (anPlayer_Push_AvFrameList(pVideoFrameList, frame_filter) < 0)
                          {
                              av_frame_free(&frame_filter);
                          }
                      }
                
                      av_frame_free(&frame_dec);
                   }
                   else
                   {
                       if (anPlayer_Push_AvFrameList(pVideoFrameList, frame_dec) < 0)
                       {
                           av_frame_free(&frame_dec);
                       }
                   }
                }
                else
                {
                    if (anPlayer_Push_AvFrameList(pVideoFrameList, frame_dec) < 0)
                    {
                        av_frame_free(&frame_dec);
                    }
                }
            }

            if (frame_dec)
            {
                av_frame_free(&frame_dec);
            }
            
            empty_loop++;
         }
         else if (pkt_special > 0)
         {
             ANRingQLF_Header_Pop(pPlayer->VideoPktQ);       
         }
      }

      if (empty_loop == 0)
      {
         av_usleep(10000);
         empty_loop = 0;
      }

      //如果文件已经读取完，也没有可以显示的了，就退出吧
      if ((ANRingQLF_Get_Count(pVideoFrameList) == 0) && (pPlayer->reach_to_end))
      {
         break;
      }

   }

quit:

   if (pPlayer->time_axis_type == TICKS_AXIS_BY_VIDEO)
   {
      pPlayer->time_axis_type = TICKS_AXIS_BY_AUDIO;
   }

   pPlayer->task_loop_video = 0;

   if (Filters_Subtext.filter_graph)
   {
      avfilter_graph_free(&(Filters_Subtext.filter_graph));
   }
   
   memset(&Filters_Subtext, 0, sizeof(Filters_Subtext));
   
   if (opts)
   {
      av_dict_free(&opts);
   }
   if (video_frame_conveterd)
   {
      av_frame_free(&video_frame_conveterd);
      video_frame_conveterd = NULL;
   }

   if (frame_ready)
   {
      av_frame_free(&(frame_ready));
      frame_ready = NULL;
   }

   if (video_convert_ctx)
   {
      sws_freeContext(video_convert_ctx);
      video_convert_ctx = NULL;
   }

   if (video_dec_ctx)
   {
      avcodec_free_context(&video_dec_ctx);
      video_dec_ctx = NULL;
   }

   if (pVideoFrameList)
   {
      anPlayer_Flush_AvFrameList(pVideoFrameList);
      ANRingQLF_Destory(&pVideoFrameList);
      pVideoFrameList = NULL;
   }

   return;
}

void anPlayer_Thread_Audio_dec(void* param)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)param;
   AVStream *audio_stream;
   AVCodecContext *audio_dec_ctx;
   int ret;
   AVPacket * new_pkt;
   AVFrame* frame_dec;
   AVFrame* frame_ready;
   AVFrame  ConvertParam;
   int64_t frame_ready_pts_ms;
   int64_t playtime_ms;
   //基准时间
   int64_t audio_start_time_ms = AV_NOPTS_VALUE;
   AVCodec *dec;
   AVDictionary *opts;
   char szErr[256];
   uint8_t* audio_buffer;
   struct SwrContext *audio_converter_ctx;
   ANRingQueneLF_h pAudioFrameList;

   //播放开始的时间，用来记录时间戳
   int64_t play_start_ms = AV_NOPTS_VALUE;
   int64_t play_start_stream = AV_NOPTS_VALUE;
   int64_t system_time_ms;
   int empty_loop = 0;

   memset(&ConvertParam, 0, sizeof(ConvertParam));

   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Param\n", __FUNCTION__, __LINE__);
      return;
   }

   pAudioFrameList = ANRingQLF_Create(128, sizeof(AVFrame*));
   if (pAudioFrameList == NULL)
   {
      MessageError("[%s:%d]Failed to create audio ring buffer\n", __FUNCTION__, __LINE__);
      goto quit;
   }

   opts = NULL;
   audio_stream = NULL;
   audio_dec_ctx = NULL;
   frame_ready = NULL;
   audio_buffer  = NULL;
   audio_converter_ctx = NULL;

   audio_stream = pPlayer->fmt_ctx->streams[pPlayer->audio_stream_idx];

   dec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
   if (!dec)
   {
      MessageError("[%s:%d]Failed to find codec [%s]\n", __FUNCTION__, __LINE__, avcodec_get_name(audio_stream->codecpar->codec_id));
      goto quit;
   }

   audio_dec_ctx = avcodec_alloc_context3(dec);
   if (!audio_dec_ctx)
   {
      MessageError("[%s:%d]Failed to allocate the %s codec contex\n", __FUNCTION__, __LINE__, avcodec_get_name(audio_stream->codecpar->codec_id));
      goto quit;
   }

   if ((ret = avcodec_parameters_to_context(audio_dec_ctx, audio_stream->codecpar)) < 0)
   {
      MessageError("[%s:%d]Failed to copy %s codec parameters to decoder context\n", __FUNCTION__, __LINE__, \
         avcodec_get_name(audio_stream->codecpar->codec_id));
      goto quit;
   }

   av_dict_set(&opts, "refcounted_frames", "1", 0);
   if ((ret = avcodec_open2(audio_dec_ctx, dec, &opts)) < 0)
   {
      av_make_error_string(szErr, sizeof(szErr), ret);
      MessageError("[%s:%d]Failed to open %s codec, %s\n", __FUNCTION__, __LINE__, \
         avcodec_get_name(audio_stream->codecpar->codec_id), szErr);
      goto quit;
   }

   audio_start_time_ms = 0;
   if (audio_stream->start_time != AV_NOPTS_VALUE)
   {
      audio_start_time_ms = (int64_t)(audio_stream->start_time * audio_stream->time_base.num * 1000) / audio_stream->time_base.den;
   }

   //
   play_start_stream = audio_start_time_ms;
   play_start_ms = anPlayer_Get_SysTime_ms() + 100;

   audio_buffer = (uint8_t *)av_malloc(1024*1024);
   if (audio_buffer == NULL)
   {
      MessageError("[%s:%d]Failed to allocate memory\n", __FUNCTION__, __LINE__);
   }

   while ((pPlayer->task_loop == 1) && (pPlayer->task_loop_audio == 1))
   {
      empty_loop = 0;
      
      if (pPlayer->pAudioDecCmdList)
      {        
         CMDList_t* pNewCmd = NULL;
      
         uv_mutex_lock(&(pPlayer->lock));
         pNewCmd = (CMDList_t*)ObjBase_List_GetHead((OBJ_BASE_t**)&(pPlayer->pAudioDecCmdList));
         uv_mutex_unlock(&(pPlayer->lock));
         
         if (pNewCmd)
         {
            if (pNewCmd->Code == CAVQUE_CMD_RESET)
            {
               av_frame_free(&frame_ready);
               frame_ready = NULL;
               
               anPlayer_Flush_AvFrameList(pAudioFrameList);
               anPlayer_Flush_AvPacketList(pPlayer->AudioPktQ, 1);

               avcodec_flush_buffers(audio_dec_ctx);
               MessageError("[%s:%d] reset audio thread\n", __FUNCTION__, __LINE__);
            }

            free(pNewCmd);
         }
      }
      
      //更新时间戳
      if (pPlayer->time_axis_type == TICKS_AXIS_BY_AUDIO)
      {
         system_time_ms = anPlayer_Get_SysTime_ms();         
         anmisc_data_exchange_int64(&(pPlayer->time_axis_play), system_time_ms - play_start_ms + play_start_stream);
      }

      if (frame_ready == NULL)
      {
         frame_ready = NULL;        
         frame_ready = anPlayer_Pop_AvFrameList(pAudioFrameList);
         if (frame_ready)
         {
            if (frame_ready->pts != AV_NOPTS_VALUE)
            {
                frame_ready_pts_ms = (frame_ready->pts * audio_stream->time_base.num * 1000) / audio_stream->time_base.den;
            }
            else
            {
                frame_ready_pts_ms = (frame_ready->pkt_dts * audio_stream->time_base.num * 1000) / audio_stream->time_base.den;
            }
         }
      }

      if (frame_ready)
      {
         anmisc_data_exchange_int64(&playtime_ms, pPlayer->time_axis_play);
         if (frame_ready_pts_ms <= playtime_ms)
         {
            //渲染
            if (playtime_ms - frame_ready_pts_ms < 200)
            {               
               // MessageOutput("[%s:%d] playtime_ms=%lld, frame_ready_pts_ms=%lld\n", __FUNCTION__, __LINE__, playtime_ms, frame_ready_pts_ms);

               //必须转换了输出,如果格式一致，就不用转换
               if ((frame_ready->channels == pPlayer->AudioOutChannels) 
                  && (frame_ready->sample_rate == pPlayer->AudioOutSampleRate)
                  && (frame_ready->format == AV_SAMPLE_FMT_S16))
               {
                  if (pPlayer->AudioCb)
                  {
                     pPlayer->AudioCb(pPlayer, frame_ready->data[0], frame_ready->nb_samples * frame_ready->channels * 2, \
                                 frame_ready_pts_ms - audio_start_time_ms, pPlayer->UserData);
                  }
               }
               else
               {
                   if ((frame_ready->channels != ConvertParam.channels)
                      || (frame_ready->sample_rate != ConvertParam.sample_rate)
                      || (frame_ready->format != ConvertParam.format))
                   {
                       if (audio_converter_ctx)
                       {
                          swr_free(&audio_converter_ctx);
                          audio_converter_ctx = NULL;
                       }

                       memset(&ConvertParam, 0, sizeof(ConvertParam));
                   }

                  if(audio_converter_ctx == NULL)
                  {
                     uint64_t layout = AV_CH_LAYOUT_MONO;
                     int64_t dec_channel_layout;

                     if (pPlayer->AudioOutChannels == 2)
                     {
                        layout = AV_CH_LAYOUT_STEREO;
                     }

                     dec_channel_layout =
                         (frame_ready->channel_layout && av_frame_get_channels(frame_ready) == av_get_channel_layout_nb_channels(frame_ready->channel_layout)) ?
                         frame_ready->channel_layout : av_get_default_channel_layout(av_frame_get_channels(frame_ready));

                     audio_converter_ctx = swr_alloc_set_opts(\
                        audio_converter_ctx,
                        layout, AV_SAMPLE_FMT_S16, pPlayer->AudioOutSampleRate, //dst
                        dec_channel_layout, (AVSampleFormat)frame_ready->format, frame_ready->sample_rate, //src
                        0, NULL);
                     swr_init(audio_converter_ctx);

                     ConvertParam.channels = frame_ready->channels;
                     ConvertParam.sample_rate = frame_ready->sample_rate;
                     ConvertParam.format = frame_ready->format;
                  }
                  
                  if(audio_converter_ctx && audio_buffer)
                  {
                     uint8_t *data_out[AV_NUM_DATA_POINTERS];
                     int outsamples;
                     data_out[0] = audio_buffer;

                     //MessageError("[%s:%d] audiot format %d, layout:%d\n", __FUNCTION__, __LINE__, frame_ready->format, frame_ready->channel_layout);

                     outsamples = swr_convert(audio_converter_ctx, data_out, 1024 * 256, \
                           (const uint8_t **)frame_ready->data, frame_ready->nb_samples);
                     if (outsamples > 0)
                     {
                        if (pPlayer->AudioCb)
                        {
                           pPlayer->AudioCb(pPlayer, audio_buffer, outsamples * pPlayer->AudioOutChannels * 2, \
                                       frame_ready_pts_ms - audio_start_time_ms, pPlayer->UserData);
                        }
                     }
                  }
                  
               }
            }
            else
            {
               MessageOutput("[%s:%d] drop audio. playtime_ms[%lld] frame_ready_pts_ms[%lld]...\n", __FUNCTION__, __LINE__, playtime_ms, frame_ready_pts_ms);
            }
            //渲染完成，释放
            av_frame_free(&frame_ready);
            frame_ready = NULL;
         }
      }

      if (ANRingQLF_Get_Count(pAudioFrameList) <= 10)
      {    
         uint64_t pkt_special;
         new_pkt = (AVPacket* )ANRingQLF_Header_Get(pPlayer->AudioPktQ);
         pkt_special = (uint64_t)new_pkt;
         
         if (new_pkt && (pkt_special > 100))
         {
            avcodec_send_packet(audio_dec_ctx, new_pkt);
            
            av_packet_unref(new_pkt);
            ANRingQLF_Header_Pop(pPlayer->AudioPktQ);       
            
            frame_dec = av_frame_alloc();
            if (frame_dec == NULL)
            {
               MessageError("[%s:%d] Failed av_frame_alloc()\n", __FUNCTION__, __LINE__);
            }
            else
            {
               while ((ret = avcodec_receive_frame(audio_dec_ctx, frame_dec)) >= 0)
               {                  
                  if (anPlayer_Push_AvFrameList(pAudioFrameList, frame_dec) < 0)
                  {
                      av_frame_free(&frame_dec);
                      MessageOutput("[%s:%d] buffer overflow\n", __FUNCTION__, __LINE__);
                      break;

                  }
                  else {
                      frame_dec = av_frame_alloc();
                      if (frame_dec == NULL)
                      {
                         MessageError("[%s:%d] Failed av_frame_alloc()\n", __FUNCTION__, __LINE__);
                         break;
                      }
                  }
               }

               if (frame_dec)
               {
                   av_frame_free(&frame_dec);
               }
            }

            empty_loop++;
         }           
         else if (pkt_special > 0)
         {
             ANRingQLF_Header_Pop(pPlayer->AudioPktQ);       
         }
      }

      if (empty_loop == 0)
      {
         av_usleep(10000);
         empty_loop = 0;
      }

      //如果文件已经读取完，也没有可以显示的了，就退出吧
      if ((ANRingQLF_Get_Count(pAudioFrameList) == 0) && (pPlayer->reach_to_end))
      {
         break;
      }
   }

quit:

   if (frame_ready)
   {
      av_frame_free(&(frame_ready));
      frame_ready = NULL;
   }

   if (pPlayer->time_axis_type == TICKS_AXIS_BY_AUDIO)
   {
      pPlayer->time_axis_type = TICKS_AXIS_BY_VIDEO;
   }

   if (audio_converter_ctx)
   {
      swr_free(&audio_converter_ctx);
      audio_converter_ctx = NULL;
   }

   if (audio_buffer)
   {
      av_free(audio_buffer);
      audio_buffer = NULL;
   }

   pPlayer->task_loop_audio = 0;

   if (opts)
   {
      av_dict_free(&opts);
   }

   if (audio_dec_ctx)
   {
      avcodec_free_context(&audio_dec_ctx);
      audio_dec_ctx = NULL;
   }

   if (pAudioFrameList)
   {
      anPlayer_Flush_AvFrameList(pAudioFrameList);
      ANRingQLF_Destory(&pAudioFrameList);
      pAudioFrameList = NULL;
   }
   return;
}

void anPlayer_Thread_Reader(void* param)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)param;
   int Ret;
   int64_t TimeNow;
   AVPacket NewPacket;
   uint64_t video_st_dts = AV_NOPTS_VALUE;
   uint64_t audio_st_dts = AV_NOPTS_VALUE;
   uint64_t pkt_dts;
   int need_read_pkt;
   int reset_pts_firstframe;
   int QuitType = ANPLYAER_STOP_TYPE_USER;
   int IsNewPacketReady;
    int64_t file_start_time_ms = AV_NOPTS_VALUE;

   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Param\n", __FUNCTION__, __LINE__);
      return;
   }

   reset_pts_firstframe = 0;
   pPlayer->reach_to_end = 0;
   pPlayer->state = ANPLYAER_STATE_PAUSED;

   //创建队列
   pPlayer->VideoPktQ = ANRingQLF_Create(AVPACKET_CACHED_MAX, sizeof(AVPacket));
   pPlayer->AudioPktQ = ANRingQLF_Create(AVPACKET_CACHED_MAX, sizeof(AVPacket));

   av_init_packet(&NewPacket);
   IsNewPacketReady = 0;
   while ((pPlayer->task_loop == 1) && (pPlayer->task_loop_reader == 1))
   {
      TimeNow = av_gettime_relative();

      if (pPlayer->pReaderCmdList)
      {
         CMDList_t *pNewCmd = NULL;

         uv_mutex_lock(&(pPlayer->lock));
      
         pNewCmd = (CMDList_t *)ObjBase_List_GetHead((OBJ_BASE_t**)&(pPlayer->pReaderCmdList));
         
         uv_mutex_unlock(&(pPlayer->lock));
         
         if (pNewCmd)
         {
            if (pNewCmd->Code == READER_CMD_Reopen)
            {
               anPlayer_Close_File(pPlayer);
            }
            else if (pNewCmd->Code == READER_CMD_Pause)
            {
               pPlayer->state = ANPLYAER_STATE_PAUSED;

               if (pPlayer->EventCb)
               {
                  pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_PAUSE, 0, NULL, NULL, pPlayer->UserData);
               }

               CMDList_t *Msg = (CMDList_t *)malloc(sizeof(CMDList_t));
               if (Msg)
               {
                  Msg->Code = TIMER_CMD_PAUSE;
                  Msg->Param3 = AV_NOPTS_VALUE;
                  uv_mutex_lock(&(pPlayer->lock));
                  ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pTimerCmdList), (OBJ_BASE_t*)Msg);
                  uv_mutex_unlock(&(pPlayer->lock));
               }               
            }
            else if (pNewCmd->Code == READER_CMD_Play)
            {
               pPlayer->state = ANPLYAER_STATE_PLAYING;

               if (pPlayer->EventCb)
               {
                  pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_PLAY, 0, NULL, NULL, pPlayer->UserData);
               }

               CMDList_t *Msg = (CMDList_t *)malloc(sizeof(CMDList_t));
               if (Msg)
               {
                  Msg->Code = TIMER_CMD_START;
                  Msg->Param3 = AV_NOPTS_VALUE;
                  uv_mutex_lock(&(pPlayer->lock));
                  ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pTimerCmdList), (OBJ_BASE_t*)Msg);
                  uv_mutex_unlock(&(pPlayer->lock));
               }               
            }
            else if (pNewCmd->Code == READER_CMD_Seek)
            {
               anPlayer_Play_Seek(pPlayer, (uint64_t)(pNewCmd->Param1));
               if (IsNewPacketReady)
               {
                   av_packet_unref(&NewPacket);
                   IsNewPacketReady = 0;
               }
            }
            
            free(pNewCmd);
         }
      }

      //等待播放完成
      if (pPlayer->reach_to_end)
      {
         if (pPlayer->video_stream_idx >= 0)
         {
            if (pPlayer->task_loop_video == 1)
            {
               av_usleep(20000);
               continue;
            }
         }

         if (pPlayer->audio_stream_idx >= 0)
         {
            if (pPlayer->task_loop_audio == 1)
            {
               av_usleep(20000);
               continue;
            }
         }
         break;
      }

      if (pPlayer->fmt_ctx == NULL)
      {
         if (anPlayer_Open_File(pPlayer) == 0)
         {
            if ((pPlayer->instream_type == INSTREAM_TYPE_NET_STREAM)
               || (pPlayer->instream_type == INSTREAM_TYPE_DSHOW))

            {
               //avformat_flush(play_data->fmt_ctx);
            }

            pPlayer->time_axis_type = TICKS_AXIS_BY_TIMER;
               
            if (pPlayer->audio_stream_idx >= 0)
            {
               //play_data->time_axis_play = TICKS_AXIS_BY_AUDIO;
               pPlayer->task_loop_audio = 1;               
               if (uv_thread_create(&(pPlayer->taskHandleAudio), anPlayer_Thread_Audio_dec, pPlayer) < 0)
               {
                  MessageError("[%s:%d] can't create thread video\n", __FUNCTION__, __LINE__);
                  memset(&(pPlayer->taskHandleAudio), 0, sizeof(pPlayer->taskHandleAudio));
               }
            }
            
            if (pPlayer->video_stream_idx >= 0)
            {
               //play_data->time_axis_play = TICKS_AXIS_BY_VIDEO;
               pPlayer->task_loop_video = 1;               
               if (uv_thread_create(&(pPlayer->taskHandleVideo), anPlayer_Thread_Video_dec, pPlayer) < 0)
               {
                  MessageError("[%s:%d] can't create thread video\n", __FUNCTION__, __LINE__);
                  memset(&(pPlayer->taskHandleVideo), 0, sizeof(pPlayer->taskHandleVideo));
               }
            }

            CMDList_t *Msg = (CMDList_t *)malloc(sizeof(CMDList_t));
            if (Msg)
            {
               int64_t pts_min = AV_NOPTS_VALUE;
               int64_t pts_tmp;

               //最小pts
               pts_min = pPlayer->fmt_ctx->start_time;

               if (pts_min != AV_NOPTS_VALUE)
               {
                  pts_min = pts_min / 1000;
                  MessageOutput("[%s:%d] file start time=%lld.%03d\n", __FUNCTION__, __LINE__, pts_min/1000, (int)(pts_min%1000));
               }
               
               //找到最小pts的音视频文件，当作播放的起始点
               for (unsigned int i = 0; i  < pPlayer->fmt_ctx->nb_streams; ++i )
               {
                  AVStream* st = pPlayer->fmt_ctx->streams[i];
                  AVCodecParameters *par = st->codecpar;
                  if ((par->codec_type == AVMEDIA_TYPE_VIDEO) || (par->codec_type == AVMEDIA_TYPE_AUDIO))
                  {
                     if (st->start_time != AV_NOPTS_VALUE)
                     {
                        pts_tmp = (int64_t)(st->start_time * st->time_base.num * 1000) / st->time_base.den;

                        if (pts_min == AV_NOPTS_VALUE)
                        {
                           pts_min = pts_tmp;
                        }
                        else
                        {
                           if (pts_tmp < pts_min)
                           {
                              pts_min = pts_tmp;
                           }
                        }
                     }
                  }
               }

               Msg->Code = TIMER_CMD_SETBASE;

               if (pts_min == AV_NOPTS_VALUE)
               {
                  reset_pts_firstframe = 1;
                  Msg->Param3 = 0;

               }
               else
               {
                  Msg->Param3 = pts_min;
               }

               Msg->Param3 -= 500;

               //记录下开始时间
               file_start_time_ms = Msg->Param3;

               //找到当前pts最小的一路流
               uv_mutex_lock(&(pPlayer->lock));
               ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pTimerCmdList), (OBJ_BASE_t*)Msg);
               uv_mutex_unlock(&(pPlayer->lock));
            }
            else
            {
               MessageError("[%s:%d] Failed to alloc memory\n", __FUNCTION__, __LINE__);
            }

            /*if (pPlayer->audio_stream_idx >= 0)
            {
               pPlayer->time_axis_type = TICKS_AXIS_BY_AUDIO;
            }
            else
            {
               if (pPlayer->video_stream_idx >= 0)
               {
                  pPlayer->time_axis_type = TICKS_AXIS_BY_VIDEO;
               }
            }*/
            
            video_st_dts = AV_NOPTS_VALUE;
            audio_st_dts = AV_NOPTS_VALUE;
           
            pPlayer->reach_to_end = 0;
         }
         else
         {
            if (pPlayer->instream_type == INSTREAM_TYPE_FILE)
            {
                //如果是文件打开，失败就直接退出
                QuitType = ANPLYAER_STOP_TYPE_OPENFAILED; //文件打开失败
                break;
            }
         }
         continue;
      }
   
      need_read_pkt = 0;
      if ((pPlayer->instream_type == INSTREAM_TYPE_NET_STREAM) 
         || (pPlayer->instream_type == INSTREAM_TYPE_DSHOW))
      {
         need_read_pkt = 1;
      }
      else if (pPlayer->instream_type == INSTREAM_TYPE_FILE)
      {
         int video_pkt_count = 0;
         int audio_pkt_count = 0;
         need_read_pkt = 0;

         if (pPlayer->video_stream_idx >= 0)
         {
            video_pkt_count = ANRingQLF_Get_Count(pPlayer->VideoPktQ);

            if (video_pkt_count < 10)
            {
               need_read_pkt = 1;
            }
         }
         
         if (pPlayer->audio_stream_idx >= 0)
         {
            audio_pkt_count = ANRingQLF_Get_Count(pPlayer->AudioPktQ);
            if (audio_pkt_count < 10)
            {
               need_read_pkt = 1;
            }
         }

         //不能一直读取
        if (video_pkt_count >= AVPACKET_CACHED_MAX - 2)
        {
            need_read_pkt = 0;
        }

        if (audio_pkt_count >= AVPACKET_CACHED_MAX - 2)
        {
            need_read_pkt = 0;
        }
      }

      if (pPlayer->state == ANPLYAER_STATE_PAUSED)
      {
         need_read_pkt = 0;
         av_usleep(200000);
         continue;
      }

      if (need_read_pkt == 1)
      {
         av_init_packet(&NewPacket);
         IsNewPacketReady = 0;

         pPlayer->quit_time_base = TimeNow;
         pPlayer->quit_timeout = 50000000;

         Ret = av_read_frame(pPlayer->fmt_ctx, &NewPacket);
         if (Ret < 0)
         {
            MessageError("[%s:%d] File reach to end [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);
            if (pPlayer->instream_type == INSTREAM_TYPE_FILE)
            {
               //如果文件到尾部，就退出等待
               pPlayer->reach_to_end = 1;               
               if (pPlayer->EventCb)
               {
                  pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_PLAY_EOF, 0, "eof", NULL, pPlayer->UserData);
               }
               
                QuitType = ANPLYAER_STOP_TYPE_FINISHED;
            }
            else
            {
               //如果是直播流，断开之后尝试重新打开
               if (pPlayer->EventCb)
               {
                  pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_PLAY_EOF, 0, "reconecting", NULL, pPlayer->UserData);
               }
               anPlayer_Close_File(pPlayer);
            }
            continue;
         }
         else
         {
            IsNewPacketReady = 1;
         }
      }


      if (IsNewPacketReady)
      {
         AVStream * st = pPlayer->fmt_ctx->streams[pPlayer->video_stream_idx];
         AVRational timebase = st->time_base;
         
         if (NewPacket.stream_index == pPlayer->video_stream_idx)
         {
            if (NewPacket.dts != AV_NOPTS_VALUE)
            {
               pkt_dts = (int64_t)(NewPacket.dts * timebase.num * 1000) / timebase.den;
               if ((video_st_dts != AV_NOPTS_VALUE)  \
                  && (pPlayer->instream_type == INSTREAM_TYPE_NET_STREAM)\
                  )
               {
                  if ((pkt_dts < video_st_dts - 2000) || (pkt_dts > 2000 + video_st_dts))
                  {
                     anPlayer_Close_File(pPlayer);
                     continue;
                  }
               }

               video_st_dts = pkt_dts;
            }

            //写入数据到无锁队列
            if (anPlayer_Push_AvPacketList(pPlayer->VideoPktQ, &NewPacket) < 0)
            {
               MessageWarn("[%s:%d] video lost packet\n", __FUNCTION__, __LINE__);
            }
            av_packet_unref(&NewPacket);
            IsNewPacketReady = 0;
         }
         else if (NewPacket.stream_index == pPlayer->audio_stream_idx)
         {
            if (NewPacket.dts != AV_NOPTS_VALUE)
            {
               pkt_dts = (int64_t)(NewPacket.dts * timebase.num * 1000) / timebase.den;
               if ((audio_st_dts != AV_NOPTS_VALUE)  \
                  && (pPlayer->instream_type == INSTREAM_TYPE_NET_STREAM)\
                  )
               {
                  if ((pkt_dts < audio_st_dts - 2000) || (pkt_dts > 2000 + audio_st_dts))
                  {
                     anPlayer_Close_File(pPlayer);
                     continue;
                  }
               }

               audio_st_dts = pkt_dts;
            }

            if (anPlayer_Push_AvPacketList(pPlayer->AudioPktQ, &NewPacket) < 0)
            {
               MessageWarn("[%s:%d] audio lost packet\n", __FUNCTION__, __LINE__);
            }
            av_packet_unref(&NewPacket);
            IsNewPacketReady = 0;
         }
         else
         {            
            av_packet_unref(&NewPacket);
            IsNewPacketReady = 0;
         }
      }

      if (need_read_pkt == 0)
      {
         av_usleep(20000);
      }
   }

   
   if (IsNewPacketReady)
   {
       av_packet_unref(&NewPacket);
       IsNewPacketReady = 0;
   }
   
   pPlayer->state = ANPLYAER_STATE_STOPED;

   anPlayer_Close_File(pPlayer);

   //读取端已经全部退出了，所以可以放心全部删除
   anPlayer_Flush_AvPacketList(pPlayer->VideoPktQ, 0);
   anPlayer_Flush_AvPacketList(pPlayer->AudioPktQ, 0);

   //销毁所有数据
   ANRingQLF_Destory(&(pPlayer->VideoPktQ));
   ANRingQLF_Destory(&(pPlayer->AudioPktQ));

   pPlayer->VideoPktQ = NULL;
   pPlayer->AudioPktQ = NULL;

   uv_mutex_lock(&(pPlayer->lock));
   
   if (pPlayer->pReaderCmdList)
   {
      //暂时不考虑清空自定义数据，还没想好有哪些自定义数据
      ObjBase_FreeList_UserData((OBJ_BASE_t*)pPlayer->pReaderCmdList, NULL);
      pPlayer->pReaderCmdList = NULL;
   }

   uv_mutex_unlock(&(pPlayer->lock));

   if (pPlayer->EventCb)
   {
      const char* szQuitDesc[]={"USER","OPENFAILED","FINISHED"};
      pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_PLAY_DONE, 0, szQuitDesc[QuitType], NULL, pPlayer->UserData);
   }
      
   MessageOutput("[%s:%d] task quit\n", __FUNCTION__, __LINE__);

   return;
}

void anPlayer_Thread_Timer(void* param)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)param;
   int64_t start_pts = 0;
   int64_t cur_pts = AV_NOPTS_VALUE;
   int64_t pre_notify_pts = 0;
   int64_t start_sys_time = AV_NOPTS_VALUE;
   int64_t cur_sys_time = AV_NOPTS_VALUE;
   int timer_running = 0;
   int Speed;

   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Param\n", __FUNCTION__, __LINE__);
      return;
   }

   while ((pPlayer->task_loop == 1))
   {
      if (pPlayer->pTimerCmdList)
      {
         CMDList_t *pNewCmd = NULL;

         uv_mutex_lock(&(pPlayer->lock));
      
         pNewCmd = (CMDList_t *)ObjBase_List_GetHead((OBJ_BASE_t**)&(pPlayer->pTimerCmdList));
         
         uv_mutex_unlock(&(pPlayer->lock));
         
         if (pNewCmd)
         {
            if (pNewCmd->Code == TIMER_CMD_START)
            {
               if (pNewCmd->Param3 != AV_NOPTS_VALUE)
               {
                  start_pts = pNewCmd->Param3;
               }
            
               start_sys_time = anPlayer_Get_SysTime_ms();
               timer_running = 1;
               cur_sys_time = start_sys_time;
               
               cur_pts = (cur_sys_time - start_sys_time + start_pts);
               MessageError("[%s:%d]START pts: %lld\n", __FUNCTION__, __LINE__, cur_pts / 1000);
            }
            else if (pNewCmd->Code == TIMER_CMD_SPEED)
            {
               start_pts = cur_pts;
               start_sys_time = anPlayer_Get_SysTime_ms();
               cur_sys_time = start_sys_time;

               cur_pts = (cur_sys_time - start_sys_time + start_pts);
               MessageError("[%s:%d]TIMER_CMD_SPEED: %lld\n", __FUNCTION__, __LINE__, cur_pts / 1000);
            }
            else if (pNewCmd->Code == TIMER_CMD_SETBASE)
            {
               if (pNewCmd->Param3 != AV_NOPTS_VALUE)
               {
                  start_pts = pNewCmd->Param3;
               }
            }
            else if (pNewCmd->Code == TIMER_CMD_PAUSE)
            {
               timer_running = 0;
               cur_sys_time = anPlayer_Get_SysTime_ms();
               
               cur_pts = (cur_sys_time - start_sys_time + start_pts);
            
               start_pts = cur_pts;
               MessageError("[%s:%d]PAUSE pts: %lld\n", __FUNCTION__, __LINE__, cur_pts / 1000);
               
            }
            free(pNewCmd);
         }
     }

      //
      if (timer_running == 0)
      {
         av_usleep(200000);
         continue;
      }
      
      //设置当前系统播放时间，单位ms
      if (pPlayer->time_axis_type == TICKS_AXIS_BY_TIMER)
      {
          cur_sys_time = anPlayer_Get_SysTime_ms();
                
          Speed = pPlayer->PlaySpeed;
          
          if (Speed > 200)
              Speed = 200;
          
          if (Speed < 1)
              Speed = 1;
          cur_pts = ((cur_sys_time - start_sys_time) * Speed) / 100 + start_pts;

          anmisc_data_exchange_int64(&(pPlayer->time_axis_play), cur_pts);
      }
      else
      {
          anmisc_data_exchange_int64(&cur_pts, (pPlayer->time_axis_play));
      }

      if (pPlayer->EventCb)
      {
         if (ABS_TICKS(cur_pts, pre_notify_pts) > 200)
         {
             pPlayer->EventCb(pPlayer, ANPLYAER_EVENT_PTS_UPDATE, cur_pts, NULL, NULL, pPlayer->UserData);
             pre_notify_pts = cur_pts;
         }
      }

      av_usleep(10000);
   }

   return;
}


int ANPlayer_Init(ANPlayerLogCallback log)
{
   av_register_all();
   avdevice_register_all();
   avcodec_register_all();
   avformat_network_init();
   avfilter_register_all();
   av_log_set_level(AV_LOG_VERBOSE);
   if (log)
   {
      av_log_set_callback(log);
   }

#ifdef _WIN32
   timeBeginPeriod(5);
#else
   
#endif

   return 0;
}
int ANPlayer_Terminate()
{
#ifdef _WIN32
   timeEndPeriod(5);
#else
   
#endif

   return 0;
}

int ANPlayer_Inst_Create(ANPlayer_h* pHandle, const char* szFileName, void* pUserData)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)calloc(1, sizeof(ANPlayer_t));
   int ret = -1;
   
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] Failed to alloc memory\n", __FUNCTION__, __LINE__);
      goto exit;
   }
   

   snprintf(pPlayer->szFilename, sizeof(pPlayer->szFilename), "%s", szFileName);
   pPlayer->PlaySpeed = 100;
   pPlayer->UserData = pUserData;

   uv_mutex_init(&(pPlayer->lock));
   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);
   
   *pHandle = pPlayer;
   ret = 0;
exit:
   return ret;
}
int ANPlayer_Inst_Destory(ANPlayer_h* pHandle)
{
   ANPlayer_t* pPlayer = *(ANPlayer_t** )pHandle;

   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   ANPlayer_Inst_Stop(pPlayer);

   uv_mutex_destroy(&(pPlayer->lock));

   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);

   free(pPlayer);
   *pHandle = NULL;

   return 0;
}

int ANPlayer_Inst_Open(ANPlayer_h pHandle, uint32_t Flag)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;

   Q_UNUSED(Flag);

   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   if (pPlayer->taskHandleReader)
   {
       MessageError("[%s:%d] already open\n", __FUNCTION__, __LINE__);
       return -1;
   }
   
   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);

   pPlayer->task_loop = 1;
   pPlayer->task_loop_reader = 1;
   pPlayer->task_loop_video = 0;
   pPlayer->task_loop_audio = 0;
   
   if (uv_thread_create(&(pPlayer->taskHandleReader), anPlayer_Thread_Reader, pPlayer) < 0)
   {
      MessageError("[%s:%d] can't create thread reader\n", __FUNCTION__, __LINE__);
      return -1;
   }

   if (uv_thread_create(&(pPlayer->taskHandleTimer), anPlayer_Thread_Timer, pPlayer) < 0)
   {
      pPlayer->task_loop = 0;
      uv_thread_join(&(pPlayer->taskHandleReader));
      pPlayer->taskHandleReader = NULL;
      
      MessageError("[%s:%d] can't create thread timer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   return 0;
}

int ANPlayer_Inst_Play(ANPlayer_h pHandle, long Seek, uint32_t Flag)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   CMDList_t *Msg = NULL;
   Q_UNUSED(Flag);
   Q_UNUSED(Seek);
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }
   
   if (!pPlayer->taskHandleReader)
   {
       MessageError("[%s:%d] not running\n", __FUNCTION__, __LINE__);
       return -1;
   }

   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);

   Msg = (CMDList_t *)calloc(1, sizeof(CMDList_t));
   if (Msg)
   {
      uv_mutex_lock(&(pPlayer->lock));
   
      Msg->Code = READER_CMD_Play;
      //Msg->Param1 = Seek;
      ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pReaderCmdList), (OBJ_BASE_t*)Msg);
      
      uv_mutex_unlock(&(pPlayer->lock));
   }

   return 0;
}
int ANPlayer_Inst_Pause(ANPlayer_h pHandle)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   CMDList_t *Msg = NULL;
   
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }
   
   if (!pPlayer->taskHandleReader)
   {
       MessageError("[%s:%d] not running\n", __FUNCTION__, __LINE__);
       return -1;
   }

   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);

   Msg = (CMDList_t *)calloc(1, sizeof(CMDList_t));
   if (Msg)
   {
      uv_mutex_lock(&(pPlayer->lock));
   
      Msg->Code = READER_CMD_Pause;
      ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pReaderCmdList), (OBJ_BASE_t*)Msg);
      
      uv_mutex_unlock(&(pPlayer->lock));
   }

   return 0;
}
int ANPlayer_Inst_Stop(ANPlayer_h pHandle)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);

   if (pPlayer->task_loop)
   {
       pPlayer->task_loop = 0;
       
       uv_thread_join(&(pPlayer->taskHandleReader));
       uv_thread_join(&(pPlayer->taskHandleTimer));
   }

   memset(&pPlayer->taskHandleReader, 0, sizeof(pPlayer->taskHandleReader));
   memset(&pPlayer->taskHandleTimer, 0, sizeof(pPlayer->taskHandleTimer));
      
   uv_mutex_lock(&(pPlayer->lock));

   //这个地方得销毁资源，因为有可能有人在线程停止之后还在发数据的情况   
   if (pPlayer->pReaderCmdList)
   {
      //暂时不考虑清空自定义数据，还没想好有哪些自定义数据
      ObjBase_FreeList_UserData((OBJ_BASE_t*)pPlayer->pReaderCmdList, NULL);
      pPlayer->pReaderCmdList = NULL;
   }
   uv_mutex_unlock(&(pPlayer->lock));

   return 0;
}
int ANPlayer_Inst_Seek(ANPlayer_h pHandle, int64_t Seek, uint32_t Flag)
{
   CMDList_t *Msg = NULL;
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   Q_UNUSED(Flag);
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }
   
   if (!pPlayer->taskHandleReader)
   {
       MessageError("[%s:%d] not running\n", __FUNCTION__, __LINE__);
       return -1;
   }

   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);

   Msg = (CMDList_t *)calloc(1, sizeof(CMDList_t));
   if (Msg)
   {
      uv_mutex_lock(&(pPlayer->lock));
   
      Msg->Code = READER_CMD_Seek;
      Msg->Param1 = (int)Seek;
      ObjBase_List_AppendEnd((OBJ_BASE_t**)&(pPlayer->pReaderCmdList), (OBJ_BASE_t*)Msg);
      
      uv_mutex_unlock(&(pPlayer->lock));
   }

   return 0;
}

int ANPlayer_Inst_Get_State(ANPlayer_h pHandle)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   long state;
   
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   if (!pPlayer->taskHandleReader)
   {
       return ANPLYAER_STATE_IDLE;
   }
   
   MessageOutput("[%s:%d] debug [%s]\n", __FUNCTION__, __LINE__, pPlayer->szFilename);

   //任何时候都可以获取状态，所以用原子操作
   anmisc_data_exchange_long(&state, (long)pPlayer->state);

   return (int)state;
}

int ANPlayer_Inst_Set_VideoOutFormat(ANPlayer_h pHandle, AVPixelFormat fmt, int width, int height)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   pPlayer->VideoOutFmt = fmt;
   pPlayer->VideoOutWidth = width;
   pPlayer->VideoOutHeight = height;

   return 0;
}
int ANPlayer_Inst_Set_AudioOutFormat(ANPlayer_h pHandle, int channel, int samplerate)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   pPlayer->AudioOutSampleRate = samplerate;
   pPlayer->AudioOutChannels = channel;

   return 0;
}
int ANPlayer_Inst_Set_PlayCallback(ANPlayer_h pHandle, ANPlayer_Video_CB VideoCb, ANPlayer_Audio_CB AudioCB, ANPlayer_Event_CB EvtCb)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   pPlayer->VideoCb = VideoCb;
   pPlayer->AudioCb = AudioCB;
   pPlayer->EventCb = EvtCb;

   return 0;
}

int ANPlayer_Inst_Set_Option(ANPlayer_h pHandle, ANPLAYER_SOPT_CODE OptCode, int Param1, void* Param2)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   Q_UNUSED(Param1);
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }

   if (OptCode == ANPLYAER_SOPT_SUBTEXT_FILE)
   {
       if (Param2)
       {
          strcpy(pPlayer->szSubtextFilter, (char *)Param2);
       }
   }

   return 0;
}
int ANPlayer_Inst_Get_Option(ANPlayer_h pHandle, ANPLAYER_GOPT_CODE OptCode, int Param1, void* Param2)
{
   ANPlayer_t* pPlayer = (ANPlayer_t*)pHandle;
   Q_UNUSED(Param1);
   Q_UNUSED(Param2);
   Q_UNUSED(OptCode);
   if (pPlayer == NULL)
   {
      MessageError("[%s:%d] NULL Pointer\n", __FUNCTION__, __LINE__);
      return -1;
   }
   
   return 0;
}



