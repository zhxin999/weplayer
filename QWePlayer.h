#ifndef QWEPLAYER_H
#define QWEPLAYER_H
#include <QWidget>
#include <QList>
#include <QListWidget>
#include <QTimer>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include "CWeSlider.h"
#include "CWeWidgetMisc.h"

#include "anPlayer.h"
#include "CAudioDevs.h"
#include "CWeVideoRender.h"

namespace Ui {
class QWePlayer;
}

enum
{
    PLAYER_MSG_AVPLAY_BASE = 100,
    PLAYER_MSG_OPENFILE_DONE,//打开了一个文件
    PLAYER_MSG_PLAYFILE_DONE,//播放完成一个文件
    PLAYER_MSG_VIDEO_DBCLICK, // 视频区域双击
    PLAYER_MSG_FULLSCREEN_TOGGLE, //全屏按钮事件
    PLAYER_MSG_PLAYPAUSE,//暂停
    PLAYER_MSG_PLAYRESUME,//继续播放
    PLAYER_MSG_MAX
};

enum
{
    PLAYER_LIST_LOOP = 0, //列表循环
    PLAYER_FILE_LOOP, //单文件循环
    PLAYER_LIST_ONCE, //列表文件一次
    PLAYER_FILE_ONCE, //单文件播放一次
    PLAYER_LOOP_MAX
};

enum
{
    PLAYER_MODE_SIMPLE = 0, //极简单模式
    PLAYER_MODE_FULL, //完整模式
    PLAYER_MODE_MAX
};

enum
{
    RENDER_MODE_GLI420 = 0,
    RENDER_MODE_GLRGBA,
    RENDER_MODE_SOFT,
    RENDER_MODE_MAX
};

class PLAYFILE_ITEM
{
public:
    QString m_strFile;
    int m_Type; // 0:正常文件，1，前缀，2，后缀
    QString m_Subtext; //"auto", "none","文件名字"
    int m_StartPos;
};


class PLAYFILE_INFO
{
public:
    QString filename;
    QString state;
    int list_indx; 
    int total_length; //总的时间，ms
    int cur_time; //当前播放时间
};

class QWePlayer : public QWidget
{
    Q_OBJECT

public:
    explicit QWePlayer(QWidget *parent = 0);
    ~QWePlayer();

    static int SystemInit(ANPlayerLogCallback fflog);
    static int SystemTerminate();

    static int ANPlayer_Video_Callback(ANPlayer_h pPlayer, AVFrame* frame, int64_t pts, void* UserData);
    static int ANPlayer_Event_Callback(ANPlayer_h pPlayer, ANPLAYER_EVENT_CODE EvtCode, int64_t Param1, const char* Param2, void* Param3, void* UserData);
    static int ANPlayer_Audio_Callback(ANPlayer_h pPlayer, uint8_t* pData, int DataLen, int64_t pts, void* UserData);

    //设置显示模式
    void SetSkinMode(int mode);
    void SetRenderMode(int mode);

    //设置渲染
    void SetKeepAspect(bool bKeep);
    bool GetKeepAspect();

    int GetRenderMode();
    void SetRenderModeCfg(int mode);

    //在显示模式之外的强制显示
    void ToggleFullScreen(bool bTrue);

    void LoadSetting(QString& path);
    void SaveSetting(QString& path);

    int PlayListAdd(QString filename, QString option, int index);
    int PlayListDelete(int index);
    int PlayListSetCur(int indx);
    int PlayListClean();
    int PlayListGet(QVariantList & listdata);

    int PlayListGetName(int indx, QString& filename);

    int PlayStop(bool AutoPlayNext);
    int PlayStart();
    int PlayPause();
    int PlaySeek(int seekMs);
    int PlayNext();
    int PlayPre();

    //获取当前正在播放的信息
    int GetPlayInfo(PLAYFILE_INFO& info);

    int SetVolume(bool mute, int Value);

    void UserResize();

private:
    ANPlayer_h OpenFile(PLAYFILE_ITEM& item);
    int CloseFile(ANPlayer_h handle);
    int GetSubtextFileByVideo(QString& strVideoFile, QString& strSubtextFile);
    void VolumeFilterCopy(unsigned char* szDst, unsigned char* szSrc, int Size, int Volume);
    void SetPlayTimeText(int Total, int Current);

    void AddVideoFileToPlayList(QString filename, QString option,int Pos);

    int GetCurPlayItemState(int state);
    void SetCurPlayItemState(int idx, int state);

protected:
  virtual void resizeEvent(QResizeEvent *);
  virtual bool eventFilter(QObject *,QEvent *);

signals:
    void sgnlPlayerMsg(int MsgCode, qint64 Param1, QString Param2, void* Param3);

    //这个是内部使用的，外面不要调用...
    void NotifyUIMsg(int MsgCode, qint64 Param1, void* Param2);

private slots:
    void on_Notify_UIMsg(int MsgCode, qint64 Param1, void* Param2);

    void on_btnShowPlayList_clicked();

    void on_btnFullscreen_clicked();

    void on_btnPlayPause_clicked();

    void on_btnPlayStop_clicked();

    void on_btnPlayNext_clicked();

    void on_btnVol_clicked();

    void on_btnListAdd_clicked();

    void on_btnListDel_clicked();

    void on_btnListClean_clicked();

    void on_listPlay_itemDoubleClicked(QListWidgetItem *item);

    void on_sliderVol_valueChanged(int value);

    void on_playprocess_clicked(qint64 pos);

    void on_timer_Ticks();

    void on_timer_Ticks_fast();

private:
    Ui::QWePlayer *ui;

    bool m_FullScreen;

    CWeSlider* m_Slider;

    CWarningText* m_WarnningTopLeft;
    CWarningText* m_WarnningBottom;

    ANPlayer_h m_Player;
    CSoftRgbRender* m_SoftRgbRender;
    CGLI420Render* m_GlI420Render;
    CGLRGBARender* m_GlRGBARender;

    //设计这个是为了考虑将来强制播放片头或者片尾广告片
    QList<PLAYFILE_ITEM> m_PlayScenaro;
    int m_CurPlayScenaroIndex;

    //播放时间相关的
    int64_t m_TotalTime;
    int64_t m_PlayStartPts;
    int64_t m_CurPlayPts;
    //音频播放器
    PlaybackDev m_AudioPlayback;

    int m_AudioChannel;
    int m_AudioSamplerate;

    int m_VolumeValue;
    bool m_IsMuteAudio;

    int m_LongSeek;
    QTimer *m_timer;
    QTimer *m_timer_fast;

    int m_LoopMode;
    bool m_AutoPlayNext;

    int m_PlayerMode;

    int m_RenderMode;
    bool m_KeepAspect;

    QList<ANPlayer_h> m_HandleList;
    QString m_defCfgFile;
};

#endif // QWEPLAYER_H
