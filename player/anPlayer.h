#ifndef __AN_PLAYER_H__
#define __AN_PLAYER_H__

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}


typedef enum
{
   ANPLYAER_EVENT_OPEN_FAILED,
   ANPLYAER_EVENT_OPEN_DONE,
   ANPLYAER_EVENT_PLAY_EOF,
   ANPLYAER_EVENT_PLAY_DONE,
   ANPLYAER_EVENT_SEEK_DONE,
   ANPLYAER_EVENT_PAUSE,
   ANPLYAER_EVENT_PLAY,
   ANPLYAER_EVENT_SUBTEXT_FAILED,
   ANPLYAER_EVENT_PTS_UPDATE,
   ANPLYAER_EVENT_CODE_MAX
} ANPLAYER_EVENT_CODE;

typedef enum
{
   ANPLYAER_SOPT_PLAYSPEED = 0,
   ANPLYAER_SOPT_SUBTEXT_FILE,

   ANPLYAER_SOPT_CODE_MAX
} ANPLAYER_SOPT_CODE;

typedef enum
{

   ANPLYAER_GOPT_CODE_MAX
} ANPLAYER_GOPT_CODE;

typedef enum
{
   ANPLYAER_STATE_IDLE = 0,
   ANPLYAER_STATE_OPENFAILED,
   ANPLYAER_STATE_OPENDONE,
   ANPLYAER_STATE_PLAYING,
   ANPLYAER_STATE_PAUSING,
   ANPLYAER_STATE_SEEKING,
   ANPLYAER_STATE_PAUSED,
   ANPLYAER_STATE_STOPPING,
   ANPLYAER_STATE_STOPED,
   ANPLYAER_STATE_CODE_MAX
} ANPLAYER_STATE_CODE;

enum
{
   ANPLYAER_STOP_TYPE_USER = 0,
   ANPLYAER_STOP_TYPE_OPENFAILED,
   ANPLYAER_STOP_TYPE_FINISHED,
} ;

typedef void* ANPlayer_h;


typedef int(*ANPlayer_Video_CB)(ANPlayer_h pPlayer, AVFrame* frame, int64_t pts, void* UserData);
typedef int(*ANPlayer_Event_CB)(ANPlayer_h pPlayer, ANPLAYER_EVENT_CODE EvtCode, int64_t Param1, const char* Param2, void* Param3, void* UserData);
typedef int(*ANPlayer_Audio_CB)(ANPlayer_h pPlayer, uint8_t* pData, int DataLen, int64_t pts, void* UserData);

typedef void (* ANPlayerLogCallback)(void* ptr, int level, const char* fmt, va_list vl);


int ANPlayer_Init(ANPlayerLogCallback log);
int ANPlayer_Terminate();


int ANPlayer_Inst_Create(ANPlayer_h* pHandle, const char* szFileName, void* pUserData);
int ANPlayer_Inst_Destory(ANPlayer_h* pHandle);

//检测文件是否能打开，并同时有参数控制是否显示第一帧画面
int ANPlayer_Inst_Detect(ANPlayer_h pHandle, uint32_t Flag);

int ANPlayer_Inst_Open(ANPlayer_h pHandle, uint32_t Flag);
int ANPlayer_Inst_Play(ANPlayer_h pHandle, long Seek, uint32_t Flag);
int ANPlayer_Inst_Pause(ANPlayer_h pHandle);
int ANPlayer_Inst_Stop(ANPlayer_h pHandle);
int ANPlayer_Inst_Seek(ANPlayer_h pHandle, int64_t Seek, uint32_t Flag);

int ANPlayer_Inst_Get_State(ANPlayer_h pHandle);


int ANPlayer_Inst_Set_VideoOutFormat(ANPlayer_h pPlayer, AVPixelFormat fmt, int width, int height);
int ANPlayer_Inst_Set_AudioOutFormat(ANPlayer_h pPlayer, int channel, int samplerate);
int ANPlayer_Inst_Set_PlayCallback(ANPlayer_h pPlayer, ANPlayer_Video_CB VideoCb, ANPlayer_Audio_CB AudioCB, ANPlayer_Event_CB EvtCb);

int ANPlayer_Inst_Set_Option(ANPlayer_h pPlayer, ANPLAYER_SOPT_CODE OptCode, int Param1, void* Param2);
int ANPlayer_Inst_Get_Option(ANPlayer_h pPlayer, ANPLAYER_GOPT_CODE OptCode, int Param1, void* Param2);


#endif

