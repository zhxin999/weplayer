#include "QWePlayer.h"
#include "ui_QWePlayer.h"
#include "anIncludes.h"
#include "anLogs.h"
#include<QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QMimeData>
#include <QScrollBar>
#include <QObject>
#include <QDomComment>
#include <QJsonArray>

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#define PROGRESS_BAR_HEIGHT         20
#define TOOL_BAR_HEIGHT             54
#define PLAYPROCESS_BAR_HEIGHT      12
#define PLAYLIST_BAR_WIDTH          200

#define AUDIO_SAMPLE_RATE        48000

enum
{
    UI_MSG_AVPLAY_BASE = 100,
    UI_MSG_OPENFILE_DONE,
    UI_MSG_OPENFILE_FAILED,
    UI_MSG_PLAYFILE_DONE,
    UI_MSG_PLAYFILE_OEF,
    UI_MSG_PTS_UPDATE,
    UI_MSG_SEEK_DONE,
    UI_MSG_SUBTEXT_FAILED,

    UI_MSG_MAX
};

enum
{
    UI_STOP_REASON_PLAYDONE = 0, //自动播放完成
    UI_STOP_REASON_OPENFAILED,
    UI_STOP_REASON_USERSTOP,
    UI_STOP_MAX
};


enum
{
    PLIST_STATE_IDLE = 0,
    PLIST_STATE_PLAYREADY,
    PLIST_STATE_PLAYING,
    PLIST_STATE_PLAYDONE,
    PLIST_STATE_MAX
};

enum
{
    PLIST_UDATA_FULLNAME = Qt::UserRole + 1,
    PLIST_UDATA_SUBTEXTFILE,
    PLIST_UDATA_FILEOPTION, //文件打开的一些参数
    PLIST_UDATA_PLAYSTATE  //表示这个文件就是正在播放的文件
};

int QWePlayer::SystemInit(ANPlayerLogCallback fflog)
{
    ANPlayer_Init(fflog);
    return 0;
}
int QWePlayer::SystemTerminate()
{
    ANPlayer_Terminate();
    return 0;
}

QWePlayer::QWePlayer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QWePlayer)
{
    ui->setupUi(this);

    m_Slider = new CWeSlider(this);
    m_Slider->show();
    m_Slider->setFocusPolicy(Qt::NoFocus);

    m_WarnningTopLeft = new CWarningText(this);
    m_WarnningTopLeft->hide();
    m_WarnningBottom = new CWarningText(this);
    m_WarnningBottom->hide();
    m_WarnningBottom->setMargin(2);
    m_WarnningBottom->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_GlI420Render = NULL;
    m_GlRGBARender = NULL;
    m_SoftRgbRender = NULL;

    SetRenderMode(RENDER_MODE_GLI420);

    //设置背景色
    this->setAttribute(Qt::WA_StyledBackground,true);

    ui->frmPlayCtrlBar->setAttribute(Qt::WA_TransparentForMouseEvents,false);

    ui->frmPlayCtrlBar->installEventFilter(this);
    this->installEventFilter(this);

    connect(this, SIGNAL(NotifyUIMsg(int, qint64, void*)), this, SLOT(on_Notify_UIMsg(int, qint64, void*)));
    connect(m_Slider, SIGNAL(anqSliderClicked(qint64)), this, SLOT(on_playprocess_clicked(qint64)));

    m_FullScreen =false;

    QString strValue = gPlayCfgData->GetNodeAttribute("UserConfig/Audio/Volume", "value", "80");
    m_VolumeValue = strValue.toInt();
    if (m_VolumeValue < 0)
        m_VolumeValue = 0;
    if (m_VolumeValue > 100)
        m_VolumeValue = 100;

    ui->sliderVol->setValue(m_VolumeValue);

    m_LongSeek = 0;

    m_timer=new QTimer(this);
    connect(m_timer,SIGNAL(timeout()),this,SLOT(on_timer_Ticks()));
    m_timer->start(1000);

    m_timer_fast=new QTimer(this);
    connect(m_timer_fast,SIGNAL(timeout()),this,SLOT(on_timer_Ticks_fast()));
    m_timer_fast->start(100);

    m_IsMuteAudio = false;

    m_LoopMode = PLAYER_LIST_LOOP;
    m_AutoPlayNext = true;
    
    m_Slider->setRange(0, 1);
    m_Slider->setValue(0);

    m_Player = NULL;
    m_PlayerMode = PLAYER_MODE_FULL;

    ui->listPlay->verticalScrollBar()->setStyleSheet("QScrollBar:vertical\
    {\
        width:10px;\
        background:rgba(0,0,0,0);\
        margin:0px,0px,0px,0px;\
        padding-top:0px; \
        padding-bottom:0px;\
    }\
    QScrollBar::handle:vertical\
    {\
        width:8px;\
        background:#808080;\
        min-height:20;\
    }\
    QScrollBar::handle:vertical:hover\
    {\
        width:8px;\
        background:#00B1EA; \
        min-height:20;\
    }\
    QScrollBar::sub-page:vertical {\
     background:rgba(50,50,50,255); \
    }\
    QScrollBar::add-page:vertical {\
     background:rgba(50,50,50,255); \
    }");
    m_HandleList.clear();

    ui->btnShowPlayList->setStyleSheet("QPushButton{ color: #00B1EA; }\
                                  QPushButton:hover{ color: #00B1EA; }\
                                  QPushButton:pressed{ color: rgb(167, 167, 167); }");

}

QWePlayer::~QWePlayer()
{
    PlayStop(false);

    if (m_GlI420Render)
    {
        delete m_GlI420Render;
        m_GlI420Render = NULL;
    }
    if (m_GlRGBARender)
    {
        delete m_GlRGBARender;
        m_GlRGBARender = NULL;
    }
    if (m_SoftRgbRender)
    {
        delete m_SoftRgbRender;
        m_SoftRgbRender = NULL;
    }
    delete ui;
}
int QWePlayer::ANPlayer_Video_Callback(ANPlayer_h pPlayer, AVFrame* frame, int64_t pts, void* UserData)
{
    QWePlayer* pObjs = (QWePlayer*)UserData;

   // return 0;

    if (pObjs)
    {
        if (pObjs->m_GlI420Render)
        {
            pObjs->m_GlI420Render->PlayOneFrame( \
                    frame->data[0], frame->linesize[0], \
                    frame->data[1], frame->linesize[1], \
                    frame->data[2], frame->linesize[2], \
                    frame->width, frame->height);
        }
        else if (pObjs->m_GlRGBARender)
        {
            pObjs->m_GlRGBARender->PlayOneFrame( \
                    frame->data[0], frame->linesize[0], \
                    frame->width, frame->height);
        }
        else if (pObjs->m_SoftRgbRender)
        {
            pObjs->m_SoftRgbRender->PlayOneFrame( \
                    frame->data[0], frame->linesize[0], \
                    frame->width, frame->height);
        }
    }

    return 0;
}
int QWePlayer::ANPlayer_Event_Callback(ANPlayer_h pPlayer, ANPLAYER_EVENT_CODE EvtCode, int64_t Param1, const char* Param2, void* Param3, void* UserData)
{
    QWePlayer* pObjs = (QWePlayer*)UserData;
    if (pObjs == NULL)
    {
       return 0;
    }

    if (EvtCode == ANPLYAER_EVENT_OPEN_DONE)
    {
       AVFormatContext *fmt_ctx = (AVFormatContext *)Param3;
       AVStream* stVideo = NULL;
       AVStream* stAudio = NULL;
       AVStream* st;
       unsigned int i;
       if (fmt_ctx)
       {
          //int64_t total_Time = fmt_ctx->duration / AV_TIME_BASE;
          int64_t video_time = 0;
          int64_t audio_time = 0;
          int64_t video_time_start = 0;
          int64_t audio_time_start = 0;

          pObjs->m_TotalTime = fmt_ctx->duration/1000;

          pObjs->m_PlayStartPts = 0;

          for (i = 0; i < fmt_ctx->nb_streams; i++)
          {
             st = fmt_ctx->streams[i];
             if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
             {
                if (stVideo == NULL)
                {
                   stVideo = st;
                   video_time = (stVideo->duration * stVideo->time_base.num * 1000) / stVideo->time_base.den;
                   if (stVideo->start_time != AV_NOPTS_VALUE)
                   {
                      video_time_start = (stVideo->start_time * stVideo->time_base.num * 1000) / stVideo->time_base.den;
                   }
                }
             }
             else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
             {
                if (stAudio == NULL)
                {
                   stAudio = st;
                   audio_time = (stAudio->duration * stAudio->time_base.num * 1000) / stAudio->time_base.den;
                   if (stAudio->start_time != AV_NOPTS_VALUE)
                   {
                      audio_time_start = (stAudio->start_time * stAudio->time_base.num * 1000) / stAudio->time_base.den;
                   }
                }
             }
          }

          //总时间统计,不要用总的那个duration，感觉不准
          if ((pObjs->m_TotalTime == 0) || (pObjs->m_TotalTime == AV_NOPTS_VALUE))
          {
             pObjs->m_TotalTime = video_time;

             if (audio_time > pObjs->m_TotalTime)
             {
                pObjs->m_TotalTime = audio_time;
             }
          }

          //开始时间统计
          if (!stAudio && stVideo)
          {
             pObjs->m_PlayStartPts = video_time_start;
          }
          else if (!stVideo && stAudio)
          {
             pObjs->m_PlayStartPts = audio_time_start;
          }
          else
          {
             if (video_time_start > audio_time_start)
             {
                pObjs->m_PlayStartPts = audio_time_start;
             }
             else
             {
                pObjs->m_PlayStartPts = video_time_start;
             }
          }

          pObjs->m_CurPlayPts = pObjs->m_PlayStartPts;

          //
          emit pObjs->NotifyUIMsg(UI_MSG_OPENFILE_DONE, Param1, NULL);
       }
    }
    else if (EvtCode == ANPLYAER_EVENT_PLAY_DONE)
    {
        if (strcmp(Param2, "USER") == 0)
        {
            //打开文件失败
            emit pObjs->NotifyUIMsg(UI_MSG_PLAYFILE_DONE, UI_STOP_REASON_USERSTOP, pPlayer);
        }
        else if (strcmp(Param2, "OPENFAILED") == 0)
        {
            //播放停止
            emit pObjs->NotifyUIMsg(UI_MSG_PLAYFILE_DONE, UI_STOP_REASON_OPENFAILED, pPlayer);
        }
        else
        {
            //播放停止
            emit pObjs->NotifyUIMsg(UI_MSG_PLAYFILE_DONE, UI_STOP_REASON_PLAYDONE, pPlayer);
        }

    }
    else if (EvtCode == ANPLYAER_EVENT_SEEK_DONE)
    {
        emit pObjs->NotifyUIMsg(UI_MSG_SEEK_DONE, Param1, NULL);
    }
    else if (EvtCode == ANPLYAER_EVENT_PLAY_EOF)
    {
        //文件播放完成标记,这个标记很有用，可以用来更新历史记录里面的状态
        emit pObjs->NotifyUIMsg(UI_MSG_PLAYFILE_OEF, Param1, NULL);
    }
    else if (EvtCode == ANPLYAER_EVENT_SUBTEXT_FAILED)
    {
        //字幕解析失败
        emit pObjs->NotifyUIMsg(UI_MSG_SUBTEXT_FAILED, Param1, NULL);
    }
   else if (EvtCode == ANPLYAER_EVENT_PTS_UPDATE)
   {
      pObjs->m_CurPlayPts = Param1;
      
      emit pObjs->NotifyUIMsg(UI_MSG_PTS_UPDATE, pObjs->m_CurPlayPts, NULL);
   }

    return 0;
}
int QWePlayer::ANPlayer_Audio_Callback(ANPlayer_h pPlayer, uint8_t* pData, int DataLen, int64_t pts, void* UserData)
{
    QWePlayer* pObjs = (QWePlayer*)UserData;

    if (pObjs)
    {
       if (pObjs->m_IsMuteAudio)
        {
            memset(pData, 0, DataLen);
        }
        else
        {
           pObjs->VolumeFilterCopy(pData, pData, DataLen, pObjs->m_VolumeValue);
        }
        pObjs->m_AudioPlayback.PushData(pData, DataLen / 4);
        //audio_packet_put(&(pObjs->m_AudioPlayback.m_AudioRingBuf), pData, DataLen, (uint64_t)pts);
    }
    return 0;
}

void QWePlayer::SetSkinMode(int mode)
{
    m_PlayerMode = mode;
    if (m_PlayerMode == PLAYER_MODE_SIMPLE)
    {
        ui->frmPlayCtrlBar->hide();
        ui->frmPlayList->hide();
        m_Slider->hide();
    }
    else if (m_PlayerMode == PLAYER_MODE_FULL)
    {
        ui->frmPlayCtrlBar->show();
        m_Slider->show();
    }
    else
    {
        
    }
    
    resizeEvent(NULL);
}

//在显示模式之外的强制显示
void QWePlayer::ToggleFullScreen(bool bTrue)
{
    if (m_PlayerMode == PLAYER_MODE_FULL)
    {
        if (bTrue)
        {
            ui->frmPlayCtrlBar->hide();
            ui->frmPlayList->hide();
            m_Slider->hide();
        }
        else
        {
            ui->frmPlayCtrlBar->show();
            m_Slider->show();
        }
    }

    resizeEvent(NULL);
}

void QWePlayer::SetRenderMode(int mode)
{
    m_RenderMode = mode;
    if (m_RenderMode == RENDER_MODE_GLI420)
    {
        if (m_SoftRgbRender)
        {
            delete m_SoftRgbRender;
            m_SoftRgbRender = NULL;
        }
        if (m_GlRGBARender)
        {
            delete m_GlRGBARender;
            m_GlRGBARender = NULL;
        }

        m_GlI420Render = new CGLI420Render(this);
        m_GlI420Render->show();

        //不处理鼠标事件，保证能够拖拽
        m_GlI420Render->setAttribute(Qt::WA_TransparentForMouseEvents,false);

    }
    else if (m_RenderMode == RENDER_MODE_GLRGBA)
    {
        if (m_SoftRgbRender)
        {
            delete m_SoftRgbRender;
            m_SoftRgbRender = NULL;
        }
        if (m_GlI420Render)
        {
            delete m_GlI420Render;
            m_GlI420Render = NULL;
        }

        m_GlRGBARender = new CGLRGBARender(this);
        m_GlRGBARender->show();

        //不处理鼠标事件，保证能够拖拽
        m_GlRGBARender->setAttribute(Qt::WA_TransparentForMouseEvents,false);

    }
    else
    {
        if (m_GlI420Render)
        {
            delete m_GlI420Render;
            m_GlI420Render = NULL;
        }
        if (m_GlRGBARender)
        {
            delete m_GlRGBARender;
            m_GlRGBARender = NULL;
        }

        m_SoftRgbRender = new CSoftRgbRender(this);
        m_SoftRgbRender->show();

        //不处理鼠标事件，保证能够拖拽
        m_SoftRgbRender->setAttribute(Qt::WA_TransparentForMouseEvents,false);
    }
}

int QWePlayer::GetRenderMode()
{
    if (m_RenderMode == RENDER_MODE_GLI420)
        return 1;
    if (m_RenderMode == RENDER_MODE_GLRGBA)
        return 2;

    return 0;
}

void QWePlayer::SetRenderModeCfg(int mode)
{
    if (mode == 1)
    {
        m_RenderMode = RENDER_MODE_GLI420;
    }
    else if (mode == 2)
    {
        m_RenderMode = RENDER_MODE_GLRGBA;
    }
    else
    {
        m_RenderMode = RENDER_MODE_SOFT;
    }
}

void QWePlayer::UserResize()
{
    int toolbar_height = TOOL_BAR_HEIGHT;

    //m_GlI420Render->raise();
    ui->frmPlayCtrlBar->raise();
    ui->frmPlayList->raise();
    m_Slider->raise();
    m_WarnningTopLeft->raise();
    m_WarnningBottom->raise();

    if (ui->frmPlayCtrlBar->isHidden())
        toolbar_height = 0;

    m_Slider->setGeometry(0, this->height() - toolbar_height - PLAYPROCESS_BAR_HEIGHT/2, this->width(), PLAYPROCESS_BAR_HEIGHT);
    ui->frmPlayCtrlBar->setGeometry(0, this->height() - toolbar_height, this->width(), toolbar_height);
    ui->frmPlayList->setGeometry(0, 0, PLAYLIST_BAR_WIDTH, this->height() - toolbar_height);

    if (m_GlI420Render)
    {
        m_GlI420Render->setGeometry(0, 0, this->width(), this->height() - toolbar_height);
    }
    else if (m_GlRGBARender)
    {
        m_GlRGBARender->setGeometry(0, 0, this->width(), this->height() - toolbar_height);
    }
    else if (m_SoftRgbRender)
    {
        m_SoftRgbRender->setGeometry(0, 0, this->width(), this->height() - toolbar_height);
    }

    if (ui->frmPlayList->isHidden())
    {
        m_WarnningTopLeft->setGeometry(0, 0, m_WarnningBottom->width(), m_WarnningTopLeft->height());
        m_WarnningBottom->setGeometry(0, m_Slider->geometry().top() - GetScaledSize(30), m_WarnningBottom->width(), GetScaledSize(30));
    }
    else
    {
        m_WarnningTopLeft->setGeometry(ui->frmPlayList->geometry().right(), 0, m_WarnningTopLeft->width(), m_WarnningTopLeft->height());
        m_WarnningBottom->setGeometry(ui->frmPlayList->geometry().right(), m_Slider->geometry().top() - GetScaledSize(30), m_WarnningBottom->width(), GetScaledSize(30));
    }
}

void QWePlayer::resizeEvent(QResizeEvent *evt)
{
    UserResize();
}
bool QWePlayer::eventFilter(QObject *watched, QEvent *event)
{
    QEvent::Type evtType = event->type();

    if(evtType ==QEvent::MouseButtonDblClick)
    {
        if (watched == this)
        {
            emit sgnlPlayerMsg(PLAYER_MSG_VIDEO_DBCLICK, 0, nullptr, NULL);
        }
    }
    else if(evtType==QEvent::DragEnter)
    {
        QDragEnterEvent* dragEnterEvt = static_cast<QDragEnterEvent *>(event);
        if (watched == this)
        {
            if (dragEnterEvt->mimeData()->hasUrls())
            {
                QList<QUrl> urls = dragEnterEvt->mimeData()->urls();
                if (urls.length() > 0)
                {
                    QUrl fileUrl = urls[0];
                    if (fileUrl.isLocalFile())
                    {
                        QString fileName = fileUrl.fileName();
                        if (gPlayCfgData->IsSupportVideoFile(fileName) == true)
                        {
                            //MessageOutput("[%s:%d] is local file done[%s] \n", __FUNCTION__, __LINE__, fileUrl.fileName().toStdString().c_str());
                            dragEnterEvt->acceptProposedAction(); //可以在这个窗口部件上拖放对象
                            //dragEnterEvt->setDropAction(Qt::LinkAction);
                            //qDebug()<<"drap current..."<<fileUrl;
                            return true;
                        }
                    }
                }

            }
        }
    }
    else if(evtType==QEvent::Drop)
    {
        QDropEvent* dropEvt = static_cast<QDropEvent *>(event);
        if (watched == this)
        {
            if (dropEvt->mimeData()->hasUrls())
            {
                QList<QUrl> urls = dropEvt->mimeData()->urls();
                if (urls.length() > 0)
                {
                    QUrl fileUrl = urls[0];
                    if (fileUrl.isLocalFile())
                    {
                        QString fileName = fileUrl.fileName();
                        if (gPlayCfgData->IsSupportVideoFile(fileName) == true)
                        {
                            //qDebug()<<"drop current..."<<fileUrl.toLocalFile();
                            if (m_Player)
                            {
                                PlayListAdd(fileUrl.toLocalFile(), nullptr, 0);
                                PlayListSetCur(0);
                                PlayStop(true);
                            }
                            else
                            {
                                PlayListAdd(fileUrl.toLocalFile(), nullptr, 0);
                                PlayListSetCur(0);
                                PlayStart();
                            }
                        }
                    }
                }

            }
        }
    }
    else if(evtType == QEvent::Wheel)
    {
        QWheelEvent  *evtMouse = static_cast<QWheelEvent  *>(event);

        if (evtMouse->angleDelta().y() > 0)
        {
            m_VolumeValue += 5;
            if (m_VolumeValue > 100)
                m_VolumeValue = 100;
        }
        else
        {
            m_VolumeValue -= 5;
            if (m_VolumeValue < 0)
                m_VolumeValue = 0;
        }
        ui->sliderVol->setValue(m_VolumeValue);
    }
    else if(evtType == QEvent::KeyRelease)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        int keycode = keyEvent->key();

        if (keycode == Qt::Key_Space)
        {
            on_btnPlayPause_clicked();
        }
        else if ((keycode == Qt::Key_Return))
        {
            if (watched == this)
            {
                emit sgnlPlayerMsg(PLAYER_MSG_VIDEO_DBCLICK, 0, nullptr, NULL);
            }
        }
        else if (keycode == Qt::Key_Up)
        {
            m_VolumeValue += 5;
            if (m_VolumeValue > 100)
                m_VolumeValue = 100;
            ui->sliderVol->setValue(m_VolumeValue);
        }
        else if (keycode == Qt::Key_Down)
        {
            m_VolumeValue += 5;
            if (m_VolumeValue > 100)
                m_VolumeValue = 100;
            ui->sliderVol->setValue(m_VolumeValue);
        }
        else if (keycode == Qt::Key_Right)
        {
           int value = m_Slider->value();
           int max = m_Slider->maximum();

           if (value < max)
           {
               QString time = gPlayCfgData->GetNodeAttribute("DefaultConfig/Player/SeekStep", "time", "1");
               int skip = time.toInt() * 1000;

               value += skip;
               if (value < max)
               {
                    PlaySeek(value);
               }
           }
        }
        else if (keycode == Qt::Key_Left)
        {
            int value = m_Slider->value();

            QString time = gPlayCfgData->GetNodeAttribute("DefaultConfig/Player/SeekStep", "time", "1");
            int skip = time.toInt() * 1000;

            value -= skip;
            if (value >= 0)
            {
                PlaySeek(value);
            }
        }
    }

    return QWidget::eventFilter(watched,event);
}

void QWePlayer::on_btnShowPlayList_clicked()
{
    if (ui->frmPlayList->isHidden())
    {
        ui->frmPlayList->show();
        ui->btnShowPlayList->setStyleSheet("QPushButton{ color: #00B1EA; }\
                                  QPushButton:hover{ color: #00B1EA; }\
                                  QPushButton:pressed{ color: rgb(167, 167, 167); }");

    }
    else
    {
        ui->frmPlayList->hide();

        ui->btnShowPlayList->setStyleSheet("QPushButton{ color: rgb(255, 255, 255); }\
                                  QPushButton:hover{ color: #00B1EA; }\
                                  QPushButton:pressed{ color: rgb(167, 167, 167); }");
    }
    resizeEvent(NULL);
}

void QWePlayer::on_btnFullscreen_clicked()
{
    emit sgnlPlayerMsg(PLAYER_MSG_FULLSCREEN_TOGGLE, 0, nullptr, NULL);
}

void QWePlayer::on_btnPlayPause_clicked()
{
    //m_WarnningBottom->setTextTimeout(QString("测试开始播放信息显示"), 2000);
    if (m_Player == NULL)
    {
        PlayStart();
    }
    else
    {
        PlayPause();
    }
}

void QWePlayer::on_btnPlayStop_clicked()
{
    PlayStop(false);
}

void QWePlayer::on_btnPlayNext_clicked()
{
    PlayNext();
}

void QWePlayer::on_btnVol_clicked()
{
    if (ui->sliderVol->isEnabled())
    {
        m_IsMuteAudio = true;
        ui->lblVol->setText("0");
        ui->sliderVol->setEnabled(false);
        ui->btnVol->setStyleSheet("QPushButton{border-image:url(:/resource/audio_mute.png);}\
                                  QPushButton:hover{border-image:url(:/resource/audio_mute_hover.png);}\
                                  QPushButton:pressed{border-image:url(:/resource/audio_mute.png);}");
    }
    else
    {
        m_IsMuteAudio = false;
        ui->sliderVol->setEnabled(true);
        ui->btnVol->setStyleSheet("QPushButton{border-image:url(:/resource/audio_vol.png);}\
                                  QPushButton:hover{border-image:url(:/resource/audio_vol_hover.png);}\
                                  QPushButton:pressed{border-image:url(:/resource/audio_vol.png);}");
        ui->lblVol->setText(QString::number(ui->sliderVol->value()));
    }
}

void QWePlayer::on_btnListAdd_clicked()
{
    //添加视频文件
    QString lastPath = gPlayCfgData->GetNodeText("UserConfig/LastFilePath", "");

    QString file_name = QFileDialog::getOpenFileName(this,"打开视频文件", lastPath,"*.*");

    if (file_name.length() > 2)
    {
        int i = file_name.lastIndexOf('/');
        QString Path = file_name.left(i);
        gPlayCfgData->SetNodeText("UserConfig/LastFilePath", Path);
        gPlayCfgData->SaveUserCfg();

        if (gPlayCfgData->IsSupportVideoFile(file_name) == false)
        {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QString("错误"));
            msgBox.setStyleSheet("background-color:white;");
            msgBox.setText(QString("不支持的文件格式"));
            msgBox.exec();
            return;
        }
        else
        {
            AddVideoFileToPlayList(file_name, nullptr, -1);
        }
    }
}

void QWePlayer::on_btnListDel_clicked()
{
    int n=ui->listPlay->count();
    for(int i=0;i<n;i++)
    {
        if (ui->listPlay->item(i)->isSelected())
        {
            QListWidgetItem *item = ui->listPlay->takeItem(i);

            if (item->data(PLIST_UDATA_PLAYSTATE).toInt() == PLIST_STATE_PLAYING)
            {
                //当前文件正在播放
                PlayStop(true);
            }

            delete item;
            break;
        }
    }
}

void QWePlayer::on_btnListClean_clicked()
{
    PlayListClean();
}

void QWePlayer::LoadSetting(QString& path)
{
    QDomDocument Doc;
    QDomElement RootNode;

    QFile file(path);
    QString error;
    int row = 0, column = 0;

    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        goto DefaultNode;
    }


    if(!Doc.setContent(&file, false, &error, &row, &column))
    {
        goto DefaultNode;
    }

    if(Doc.isNull())
    {
        goto DefaultNode;
    }

    RootNode = Doc.documentElement();

    if (RootNode.tagName() != QString("QWPlayer"))
    {
        QMessageBox::information(NULL, QString("默认配置文件格式不匹配"), QString("错误"));
        Doc.clear();
        RootNode.clear();
        goto DefaultNode;
    }

    goto GetValue;

DefaultNode:
    {
        QDomProcessingInstruction instruction = Doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
        Doc.appendChild(instruction);

        RootNode = Doc.createElement("QWPlayer");//创建根节点
        Doc.appendChild(RootNode);//添加根节点
    }
GetValue:
    //
    QDomElement elementCfg = RootNode.firstChildElement("Cfg");
    if(!elementCfg.isNull())
    {
        QDomElement eleCfgSkinMode = elementCfg.firstChildElement("SkinMode");
        if (!eleCfgSkinMode.isNull())
        {
            QString value = eleCfgSkinMode.attribute("value");
            if (value == "full")
            {
                SetSkinMode(PLAYER_MODE_FULL);
            }
            else if (value == "simple")
            {
                SetSkinMode(PLAYER_MODE_SIMPLE);
            }
        }

        QDomElement eleCfgVRender = elementCfg.firstChildElement("VRender");
        if (!eleCfgVRender.isNull())
        {
            QString value = eleCfgVRender.attribute("value");
            if (value == "yuv420")
            {
                SetRenderMode(RENDER_MODE_GLI420);
            }
            else if (value == "glrgba")
            {
                SetRenderMode(RENDER_MODE_GLRGBA);
            }
            else if (value == "soft")
            {
                SetRenderMode(RENDER_MODE_SOFT);
            }
        }
    }

    QDomElement elementPlayList = RootNode.firstChildElement("PlayList");
    if(!elementPlayList.isNull())
    {
        QString value = elementPlayList.attribute("mode");
        if (value == "listloop")
        {
            m_LoopMode = PLAYER_LIST_LOOP;
        }
        else if (value == "fileloop")
        {
            m_LoopMode = PLAYER_FILE_LOOP;
        }
        else if (value == "listonce")
        {
            m_LoopMode = PLAYER_LIST_ONCE;
        }
        else if (value == "fileonce")
        {
            m_LoopMode = PLAYER_FILE_ONCE;
        }

        //加载列表
        QDomElement elementFile = elementPlayList.firstChildElement("File");
        while(!elementFile.isNull())
        {
            QString filePath = elementFile.text();
            PlayListAdd(filePath, nullptr, -1);

            elementFile = elementFile.nextSiblingElement();
        }
    }

    //记录下这个配置文件，因为后续自动保存的时候会保存到这个位置
    m_defCfgFile = path;
}

void QWePlayer::SaveSetting(QString& path)
{
    QDomDocument doc;
    QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild(instruction);

    QDomElement root = doc.createElement("QWPlayer");//创建根节点
    doc.appendChild(root);//添加根节点

    QString strSaveFileName;
    if (path.length() < 4)
        strSaveFileName = m_defCfgFile;
    else
        strSaveFileName = path;

    if (strSaveFileName.length() < 4)
        return;

    QFile file(strSaveFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    {
        QDomElement nodeFile = doc.createElement("Cfg");
        root.appendChild(nodeFile);

        QDomElement nodeSkinMode = doc.createElement("SkinMode");
        nodeFile.appendChild(nodeSkinMode);
        if (m_PlayerMode == PLAYER_MODE_SIMPLE)
        {
            nodeSkinMode.setAttribute("value", "simple");
        }
        else
        {
            nodeSkinMode.setAttribute("value", "full");
        }

        QDomElement nodeVRender = doc.createElement("VRender");
        nodeFile.appendChild(nodeVRender);
        if (m_RenderMode == RENDER_MODE_GLRGBA)
        {
            nodeVRender.setAttribute("value", "glrgba");
        }
        else if (m_RenderMode == RENDER_MODE_SOFT)
        {
            nodeVRender.setAttribute("value", "soft");
        }
        else
        {
            nodeVRender.setAttribute("value", "yuv420");
        }
    }

    QDomElement nodePlaylist = doc.createElement("PlayList");
    root.appendChild(nodePlaylist);
    if (m_LoopMode == PLAYER_LIST_LOOP)
    {
        nodePlaylist.setAttribute("mode", "listloop");
    }
    else if (m_LoopMode == PLAYER_FILE_LOOP)
    {
        nodePlaylist.setAttribute("mode", "fileloop");
    }
    else if (m_LoopMode == PLAYER_LIST_ONCE)
    {
        nodePlaylist.setAttribute("mode", "listonce");
    }
    else if (m_LoopMode == PLAYER_FILE_ONCE)
    {
        nodePlaylist.setAttribute("mode", "fileonce");
    }
    else
    {
        nodePlaylist.setAttribute("mode", "listloop");
    }

    for(int i = 0; i < ui->listPlay->count(); i++)
    {
        QString itemPath = ui->listPlay->item(i)->data(PLIST_UDATA_FULLNAME).toString();
        if (itemPath.length() > 2)
        {
            QDomElement nodeFile = doc.createElement("File");

            QDomText strFileName = doc.createTextNode(itemPath);
            nodeFile.appendChild(strFileName);

            nodePlaylist.appendChild(nodeFile);
        }

    }

    doc.save(out, 4, QDomNode::EncodingFromTextStream);
    file.close();
}

int QWePlayer::PlayListAdd(QString filename, QString option, int index)
{
    AddVideoFileToPlayList(filename, option, index);
    //m_PlayList.push_back(item);
    return 0;
}

int QWePlayer::PlayListDelete(int index)
{
    int n=ui->listPlay->count();
    if ((index >= 0) && (index < n))
    {
        QListWidgetItem *item = ui->listPlay->takeItem(index);
        if (item)
        {
            if (item->data(PLIST_UDATA_PLAYSTATE).toInt() == PLIST_STATE_PLAYING)
            {
                //当前文件正在播放
                PlayStop(true);
            }

            delete item;
        }
        
    }

    return 0;
}

int QWePlayer::PlayListClean()
{
    m_PlayScenaro.clear();
    m_CurPlayScenaroIndex = 0;

    int n=ui->listPlay->count();
    for(int i=0;i<n;i++)
    {
        QListWidgetItem *item = ui->listPlay->takeItem(0);
        delete item;
    }

    PlayStop(false);

    return 0;
}

int QWePlayer::PlayListGet(QVariantList & listdata)
{
    int n = ui->listPlay->count();

    for (int i = 0; i<n; i++)
    {
        QListWidgetItem *item = ui->listPlay->item(i);
        if (item)
        {
            QVariantMap var;
            var.insert("file", item->data(PLIST_UDATA_FULLNAME).toString());
            listdata << var;
        }
    }

    return 0;
}

int QWePlayer::PlayListGetName(int indx, QString& filename)
{
    if ((indx < 0)||(indx >= ui->listPlay->count()))
    {
        return 0;
    }

    QListWidgetItem *item = ui->listPlay->item(indx);
    if (item)
    {
        filename = item->data(PLIST_UDATA_FULLNAME).toString();
        return 1;
    }

    return 0;
}

int QWePlayer::PlayListSetCur(int indx)
{
    if ((indx < 0)||(indx >= ui->listPlay->count()))
    {
        MessageError("[%s:%d] invalid index \n", __FUNCTION__, __LINE__);
        return -1;
    }

    m_PlayScenaro.clear();
    m_CurPlayScenaroIndex = 0;

    SetCurPlayItemState(indx, PLIST_STATE_PLAYREADY);

    return 0;
}

void QWePlayer::SetPlayTimeText(int Total, int Current)
{
    int hour, min, sec;
    int hourTotal, minTotal, secTotal;

    hour = (Current /1000) / 3600;
    min = ((Current /1000) % 3600) / 60;
    sec = (Current /1000) % 60;

    hourTotal = (Total /1000) / 3600;
    minTotal = ((Total /1000) % 3600) / 60;
    secTotal = (Total /1000) % 60;

    QString strTime;
    strTime = QString().sprintf("%02d:%02d:%02d", hour, min, sec);

    QString strText =QString("<html><head/><body><p align=\"left\"><span style=\" font-size:9pt; font-weight:400; color:#ffffff;\">");

    strText += strTime;

    strText += QString("</span><span style=\" font-size:9pt; font-weight:400; color:#A7A7A7;\">");

    strTime = QString().sprintf("/%02d:%02d:%02d", hourTotal, minTotal, secTotal);
    strText += strTime;

    strText += QString("</span></p></body></html>");
    ui->lblPlayCurTime->setText(strText);
}

int QWePlayer::GetCurPlayItemState(int state)
{
    int ret = -1;
    int n=ui->listPlay->count();

    for(int i=0;i<n;i++)
    {
        QListWidgetItem *item = ui->listPlay->item(i);
        if (item->data(PLIST_UDATA_PLAYSTATE).toInt() == state)
        {
            return i;
        }
    }

    return ret;
}
void QWePlayer::SetCurPlayItemState(int idx, int state)
{
    int n = ui->listPlay->count();

    if ((idx >= 0)&&(idx < n))
    {
        QListWidgetItem *item = ui->listPlay->item(idx);
        QString FullName = item->data(PLIST_UDATA_FULLNAME).toString();
        int shortname_idx = FullName.lastIndexOf('/');
        QString ShortName = FullName.mid(shortname_idx + 1);
        
        if (state == PLIST_STATE_PLAYING)
        {
            item->setData(PLIST_UDATA_PLAYSTATE, state);
            item->setTextColor(QColor(0, 0xB1, 0xEA));
            item->setText(QString("正在播放: ") + ShortName);
        }
        else
        {
            item->setData(PLIST_UDATA_PLAYSTATE, state);
            item->setTextColor(QColor(0xFF, 0xFF, 0xFF));
            item->setText(ShortName);                
        }
        
    }
    
    if (state == PLIST_STATE_PLAYING)
    {
        for(int i=0;i<n;i++)
        {
            if (idx != i)
            {
                QListWidgetItem *item = ui->listPlay->item(i);
                
                QString FullName = item->data(PLIST_UDATA_FULLNAME).toString();
                int shortname_idx = FullName.lastIndexOf('/');
                QString ShortName = FullName.mid(shortname_idx + 1);
                
                item->setData(PLIST_UDATA_PLAYSTATE, PLIST_STATE_IDLE);
                item->setTextColor(QColor(0xFF, 0xFF, 0xFF));
                item->setText(ShortName);                
            }
        }
    }
    else 
    {
        for(int i=0;i<n;i++)
        {
            if (idx != i)
            {
                QListWidgetItem *item = ui->listPlay->item(i);
                
                if (item->data(PLIST_UDATA_PLAYSTATE).toInt() == state)
                {
                    item->setData(PLIST_UDATA_PLAYSTATE, PLIST_STATE_IDLE);
                }
            }
        }
    }

}

void QWePlayer::AddVideoFileToPlayList(QString filename, QString option,int Pos)
{
    //添加到播放列表里面
    int i = filename.lastIndexOf('/');
    QString ShortName = filename.mid(i + 1);

    QListWidgetItem* ListItem = new QListWidgetItem();

    ListItem->setText(ShortName);
    ListItem->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    ListItem->setSizeHint(QSize(ListItem->sizeHint().width(), 50));
    ListItem->setToolTip(filename);
    ListItem->setData(PLIST_UDATA_FULLNAME, filename);
    ListItem->setData(PLIST_UDATA_FILEOPTION, option);
    ListItem->setData(PLIST_UDATA_PLAYSTATE, PLIST_STATE_IDLE);
    ListItem->setData(PLIST_UDATA_SUBTEXTFILE, QString("auto"));
    if (Pos < 0)
    {
        ui->listPlay->addItem(ListItem);
    }
    else
    {
        ui->listPlay->insertItem(Pos, ListItem);
    }
}

void QWePlayer::on_Notify_UIMsg(int MsgCode, qint64 Param1, void* Param2)
{
    if (UI_MSG_OPENFILE_DONE == MsgCode)
    {
         //文件已经打开，可以开始，更新总时间，进度条时间，当前时间
        MessageOutput("[%s:%d] UI_MSG_OPENFILE_DONE \n", __FUNCTION__, __LINE__);
        int playindx = GetCurPlayItemState(PLIST_STATE_PLAYING);
        
        if (playindx >= 0)
        {
            QListWidgetItem *item = ui->listPlay->item(playindx);
            emit sgnlPlayerMsg(PLAYER_MSG_OPENFILE_DONE, 0, item->data(PLIST_UDATA_FULLNAME).toString(), NULL);
        }
        
        SetPlayTimeText(m_TotalTime, 0);
        m_Slider->setRange(0, m_TotalTime);
        m_Slider->setValue(0);
        
        ui->btnPlayPause->setStyleSheet("QPushButton#btnPlayPause{border-image:url(:/resource/playpause.png);}"
           "QPushButton#btnPlayPause:hover{border-image:url(:/resource/playpause_hover.png);}"
           "QPushButton#btnPlayPause:pressed{border-image:url(:/resource/playpause.png);}");
        
        ANPlayer_Inst_Play(m_Player, 0, 0);
        
        m_Slider->EnableTips(true);
     }
    else if (UI_MSG_PTS_UPDATE == MsgCode)
    {
        //        
        if (m_LongSeek == 0)
        {
            m_Slider->setValue((m_CurPlayPts - m_PlayStartPts));
        }

        SetPlayTimeText(m_TotalTime, Param1);
     }
    else if (UI_MSG_PLAYFILE_DONE == MsgCode)
    {
        ANPlayer_h pPlayer = (ANPlayer_h)Param2;
        MessageOutput("[%s:%d] UI_MSG_PLAYFILE_DONE \n", __FUNCTION__, __LINE__);

        emit sgnlPlayerMsg(PLAYER_MSG_PLAYFILE_DONE, 0, nullptr, NULL);

        m_WarnningTopLeft->ShowHtmlTextTimeout(QString("完成"), 2000);
        if (pPlayer)
        {
            if (m_HandleList.indexOf(pPlayer) >= 0)
            {//如果这个还在列表里面，说明没有被调用过关闭,这个时候不要改变下一个播放顺序
                PlayStop(m_AutoPlayNext);
            }
            else
            {
                //调用过stop的，那我只要处理下是否需要播放下一个
            }
        }

        if (m_AutoPlayNext == true)
        {            
             //打开文件失败
            if (m_LoopMode == PLAYER_FILE_LOOP)
            {               
                //如果是单曲循环模式，又出现了打开错误，这个时候就必须要
                if (Param1 != UI_STOP_REASON_OPENFAILED)
                {
                    PlayStart();
                }
            }
            else  if (m_LoopMode == PLAYER_FILE_ONCE)
            {
            }
            else if (m_LoopMode == PLAYER_LIST_LOOP)
            {
                int curPlayingIndex = GetCurPlayItemState(PLIST_STATE_PLAYREADY);

                if (curPlayingIndex < 0)
                {
                    curPlayingIndex = GetCurPlayItemState(PLIST_STATE_PLAYDONE);
                    if (curPlayingIndex < 0)
                    {
                        if (m_Player && (m_Player == pPlayer))
                        {
                           curPlayingIndex = GetCurPlayItemState(PLIST_STATE_PLAYING);
                        }
                    }
                    
                    if (curPlayingIndex >= 0)
                    {
                        curPlayingIndex++;
                        if (curPlayingIndex >= ui->listPlay->count())
                        {
                            curPlayingIndex = 0;
                        }
                    }

               }

                if ((curPlayingIndex >= 0) && (curPlayingIndex < ui->listPlay->count()))
                {
                    PlayListSetCur(curPlayingIndex);
                    PlayStart();
                }
            }
            else if (m_LoopMode == PLAYER_LIST_ONCE)
            {
                int curPlayingIndex = GetCurPlayItemState(PLIST_STATE_PLAYREADY);

                if (curPlayingIndex < 0)
                {
                    curPlayingIndex = GetCurPlayItemState(PLIST_STATE_PLAYDONE);
                    if (curPlayingIndex >= 0)
                    {
                        curPlayingIndex++;
                        if (curPlayingIndex >= ui->listPlay->count())
                        {
                            //不播放了
                            curPlayingIndex = -1;
                        }
                    } 

               }

                if ((curPlayingIndex >= 0) && (curPlayingIndex < ui->listPlay->count()))
                {
                    PlayListSetCur(curPlayingIndex);
                    PlayStart();
                }
            }                
        }
    }
    else if (UI_MSG_PLAYFILE_OEF == MsgCode)
    {
        MessageOutput("[%s:%d] UI_MSG_PLAYFILE_OEF \n", __FUNCTION__, __LINE__);
    }
    else if (UI_MSG_OPENFILE_FAILED == MsgCode)
    {
        MessageOutput("[%s:%d] UI_MSG_OPENFILE_FAILED \n", __FUNCTION__, __LINE__);
        m_Slider->EnableTips(false);
        m_WarnningBottom->ShowHtmlTextTimeout(QString("打开视频文件失败"), 2000);        
     }
    else if (UI_MSG_SEEK_DONE == MsgCode)
    {
        //this->PlayStart();
        MessageOutput("[%s:%d] UI_MSG_SEEK_DONE \n", __FUNCTION__, __LINE__);
        //ANPlayer_Inst_Play(m_Player, 0, 0);

        m_AudioPlayback.Flush();
        //audio_packet_flush(&(m_AudioPlayback.m_AudioRingBuf));
        
        m_WarnningBottom->ShowHtmlTextTimeout(QString("跳转完成"), 2000);
    }
    else if (UI_MSG_SUBTEXT_FAILED == MsgCode)
    {
        MessageOutput("[%s:%d] UI_MSG_SUBTEXT_FAILED \n", __FUNCTION__, __LINE__);
        m_WarnningBottom->ShowHtmlTextTimeout(QString("字幕文件解析失败"), 2000);
    }
}
int QWePlayer::PlayStop(bool AutoPlayNext)
{
    int curPlayingIndex;

    m_AutoPlayNext = AutoPlayNext;

    if (AutoPlayNext == false)
    {
        m_WarnningTopLeft->setTextTimeout(QString("停止播放"), 1000);
    }

    //
    if (m_Player)
    {
        CloseFile(m_Player);

        m_HandleList.removeOne(m_Player);

        m_Player = NULL;
    }

    m_AudioPlayback.PlayStop();

    SetPlayTimeText(0, 0);
    m_Slider->setValue(0);
    m_Slider->setRange(0,1);

    if (m_GlI420Render)
        m_GlI420Render->ResetDisplay();
    if (m_GlRGBARender)
        m_GlRGBARender->ResetDisplay();
    if (m_SoftRgbRender)
        m_SoftRgbRender->ResetDisplay();

    ui->btnPlayPause->setStyleSheet("QPushButton#btnPlayPause{border-image:url(:/resource/playstart.png);}"
       "QPushButton#btnPlayPause:hover{border-image:url(:/resource/playstart_hover.png);}"
       "QPushButton#btnPlayPause:pressed{border-image:url(:/resource/playstart.png);}");

    curPlayingIndex = GetCurPlayItemState(PLIST_STATE_PLAYING);
    if (curPlayingIndex >= 0)
    {
        SetCurPlayItemState(curPlayingIndex, PLIST_STATE_PLAYDONE);
    }

    gPlayCfgData->SetNodeAttribute("UserConfig/Audio/Volume", "value", QString::number(m_VolumeValue));
    gPlayCfgData->SaveUserCfg();

    return 0;
}
int QWePlayer::PlayStart()
{
    PLAYFILE_ITEM FileItem;
    int curPlayIndex = GetCurPlayItemState(PLIST_STATE_PLAYREADY);

    if (m_Player != NULL)
    {
        return -1;
    }

    m_AutoPlayNext = false;

    if (curPlayIndex < 0)
    {
        return 0;
    }

    //当前scennaro已经播放完整了
    if (m_CurPlayScenaroIndex >=m_PlayScenaro.count())
    {
        m_PlayScenaro.clear();

        //取一个新的来播放
        QListWidgetItem *item = ui->listPlay->item(curPlayIndex);
        if (item)
        {
            FileItem.m_strFile = item->data(PLIST_UDATA_FULLNAME).toString();
            FileItem.m_Subtext = item->data(PLIST_UDATA_SUBTEXTFILE).toString();
            FileItem.m_Type = 0;
            FileItem.m_StartPos = 0;

            m_PlayScenaro.push_back(FileItem);
        }

        m_CurPlayScenaroIndex = 0;
    }

    if ((m_PlayScenaro.count() > 0) && (m_PlayScenaro.count() > m_CurPlayScenaroIndex))
    {
        ANPlayer_h handle;
        SetCurPlayItemState(curPlayIndex, PLIST_STATE_PLAYING);

        FileItem = m_PlayScenaro.at(m_CurPlayScenaroIndex);
        handle = OpenFile(FileItem);
        if (handle == NULL)
        {
            MessageError("[%s:%d] OpenFile failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
        else
        {
            m_Player = handle;
            m_HandleList.push_back(m_Player);

            m_AudioPlayback.SetOutputFormat(2, AUDIO_SAMPLE_RATE);
            m_AudioPlayback.PlayStart(-1, 2, AUDIO_SAMPLE_RATE);
            //audio_packet_flush(&(m_AudioPlayback.m_AudioRingBuf));
            m_AudioPlayback.Flush();

            //自动播放下一个
            m_AutoPlayNext = true;
        }
    }

    return 0;
}
int QWePlayer::PlayPause()
{
    int state;
    
    if (m_Player == NULL)
    {
        return -1;
    }

    state = ANPlayer_Inst_Get_State(m_Player);
    qDebug()<<"state:"<<state;
    //判断是暂停还是正在播放状态
    if (state == ANPLYAER_STATE_PAUSED)
    {
        ui->btnPlayPause->setStyleSheet("QPushButton#btnPlayPause{border-image:url(:/resource/playpause.png);}"
           "QPushButton#btnPlayPause:hover{border-image:url(:/resource/playpause_hover.png);}"
           "QPushButton#btnPlayPause:pressed{border-image:url(:/resource/playpause.png);}");
    
        ANPlayer_Inst_Play(m_Player, 0, 0);
        emit sgnlPlayerMsg(PLAYER_MSG_PLAYRESUME, 0, nullptr, NULL);
    }
    else
    {
        ui->btnPlayPause->setStyleSheet("QPushButton#btnPlayPause{border-image:url(:/resource/playstart.png);}"
           "QPushButton#btnPlayPause:hover{border-image:url(:/resource/playstart_hover.png);}"
           "QPushButton#btnPlayPause:pressed{border-image:url(:/resource/playstart.png);}");
    
        ANPlayer_Inst_Pause(m_Player);
        
        emit sgnlPlayerMsg(PLAYER_MSG_PLAYPAUSE, 0, nullptr, NULL);
    }

    return 0;
}
int QWePlayer::PlaySeek(int seekMs)
{
    m_LongSeek = 4;
    m_Slider->setValue((int)seekMs);

    return 0;
}
int QWePlayer::PlayNext()
{
    PlayStop(true);

    return 0;
}
int QWePlayer::PlayPre()
{
    return 0;
}

ANPlayer_h QWePlayer::OpenFile(PLAYFILE_ITEM& item)
{
    int IsFindSubtitles = 0;
    QString strSubtitleFile;
    ANPlayer_h pPlayer;

    if (item.m_Subtext == "none")
    {
       IsFindSubtitles = 0;
    }
    else if (item.m_Subtext == "auto")
    {
        if (GetSubtextFileByVideo(item.m_strFile, strSubtitleFile) == 1)
        {
            IsFindSubtitles = 1;
            qDebug()<<"User Subtext:"<<strSubtitleFile;
        }
    }
    else if (item.m_Subtext.length() > 4)
    {
        strSubtitleFile = item.m_Subtext;
        IsFindSubtitles = 1;

        qDebug()<<"User Subtext:"<<strSubtitleFile;
    }
    else
    {
       IsFindSubtitles = 0;
    }

    if (ANPlayer_Inst_Create(&pPlayer, item.m_strFile.toStdString().c_str(),this) != 0)
    {
        MessageError("[%s:%d] create player failed\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    if (m_RenderMode == RENDER_MODE_GLI420)
    {
        ANPlayer_Inst_Set_VideoOutFormat(pPlayer, AV_PIX_FMT_YUV420P, 0, 0);
    }
    else if (m_RenderMode == RENDER_MODE_GLRGBA)
    {
        ANPlayer_Inst_Set_VideoOutFormat(pPlayer, AV_PIX_FMT_RGBA, 0, 0);
    }
    else
    {
        ANPlayer_Inst_Set_VideoOutFormat(pPlayer, AV_PIX_FMT_BGRA, 0, 0);
    }
    ANPlayer_Inst_Set_AudioOutFormat(pPlayer, 2, AUDIO_SAMPLE_RATE);
    ANPlayer_Inst_Set_PlayCallback(pPlayer,   QWePlayer::ANPlayer_Video_Callback,\
                                               QWePlayer::ANPlayer_Audio_Callback, \
                                               QWePlayer::ANPlayer_Event_Callback);
    if (IsFindSubtitles == 1)
    {
        strSubtitleFile.replace(':', "\\\\:");
        strSubtitleFile = QString("subtitles=filename=") + strSubtitleFile;

       ANPlayer_Inst_Set_Option(pPlayer, ANPLYAER_SOPT_SUBTEXT_FILE, 0, (void *)strSubtitleFile.toStdString().c_str());
    }
    ANPlayer_Inst_Open(pPlayer, 0);

    m_AudioPlayback.SetOutputFormat(2, AUDIO_SAMPLE_RATE);
    m_AudioPlayback.PlayStart(-1, 2, AUDIO_SAMPLE_RATE);
    //audio_packet_flush(&(m_AudioPlayback.m_AudioRingBuf));
    m_AudioPlayback.Flush();

    return pPlayer;
}
int QWePlayer::CloseFile(ANPlayer_h handle)
{
    if (handle)
    {
        ANPlayer_Inst_Stop(handle);
        ANPlayer_Inst_Destory(&handle);
        handle = NULL;
    }

    return 0;
}

int QWePlayer::GetSubtextFileByVideo(QString& strVideoFile, QString& strSubtextFile)
{
    const char *szSubtitleEndfix[]={".srt",".ass", NULL};

    QString strDirPath;
    QString strShotrName;
    QString strFileNofix;

    if (strVideoFile.lastIndexOf('/') >= 0)
    {
        strDirPath = strVideoFile.mid(0, strVideoFile.lastIndexOf('/'));
        strShotrName = strVideoFile.mid(strVideoFile.lastIndexOf('/') + 1);
    }
    else
    {
        return 0;
    }

    if (strShotrName.lastIndexOf('.') >= 0)
    {
        strFileNofix = strShotrName.mid(0, strShotrName.lastIndexOf('.') + 1);
    }
    else
    {
        return 0;
    }

    QDir dir(strDirPath);

    foreach(QFileInfo mfi ,dir.entryInfoList())
    {
        if(mfi.isFile())
        {
            QString strEndfix;
            QString filename = mfi.fileName();
            int endfixMatch = 0;

            if (filename.lastIndexOf('.') >= 0)
            {
                strEndfix = filename.mid(filename.lastIndexOf('.'));
                if (strEndfix.length() > 0)
                {
                    int i = 0;

                    while (szSubtitleEndfix[i])
                    {
                        if (szSubtitleEndfix[i] == strEndfix)
                        {
                            endfixMatch = 1;
                            break;
                        }
                        i++;
                    }
                }
            }

            if (endfixMatch == 0)
            {
                continue;
            }

            //匹配文件名
            if (filename.indexOf(strFileNofix) == 0)
            {
                strSubtextFile = mfi.absoluteFilePath();
                return 1;
            }
        }
    }

    return 0;
}
void QWePlayer::VolumeFilterCopy(unsigned char* szDst, unsigned char* szSrc, int Size, int Volume)
{
    int i;
    short* pshortSrc = (short*)szSrc;
    short* pshortDst = (short*)szDst;
    int val;

    if (Volume > 100)
        Volume = 100;

    if (Volume < 0)
        Volume = 0;

    for (i = 0; i < Size/2; ++i)
    {
       val = ((int)(pshortSrc[0])) * Volume;

       val = val/100;

       *pshortDst = (short)val;
       pshortDst++;
       pshortSrc++;
    }
}

void QWePlayer::on_listPlay_itemDoubleClicked(QListWidgetItem *item)
{
    //切换到当前位置播放

    if (item->data(PLIST_UDATA_PLAYSTATE).toInt() != PLIST_STATE_PLAYING)
    {
        if (m_Player)
        {
            PlayListSetCur(ui->listPlay->row(item));
            PlayStop(true);
        }
        else
        {
            PlayListSetCur(ui->listPlay->row(item));
            PlayStart();
        }
    }

}
int QWePlayer::SetVolume(bool mute, int Value)
{
    //m_Slider
    if (mute)
    {
         //已经静音了，啥也不做
        if (m_IsMuteAudio == false)
        {
            m_IsMuteAudio = true;
            ui->lblVol->setText("0");
            ui->sliderVol->setEnabled(false);
            ui->btnVol->setStyleSheet("QPushButton{border-image:url(:/resource/audio_mute.png);}\
                                  QPushButton:hover{border-image:url(:/resource/audio_mute_hover.png);}\
                                  QPushButton:pressed{border-image:url(:/resource/audio_mute.png);}");
        }
    }
    else
    {
        if (m_IsMuteAudio == true)
        {
            m_IsMuteAudio = false;
            ui->sliderVol->setEnabled(true);
            ui->btnVol->setStyleSheet("QPushButton{border-image:url(:/resource/audio_vol.png);}\
                                  QPushButton:hover{border-image:url(:/resource/audio_vol_hover.png);}\
                                  QPushButton:pressed{border-image:url(:/resource/audio_vol.png);}");
            ui->lblVol->setText(QString::number(ui->sliderVol->value()));
        }
        else
        {
            //设置新的音量值
            ui->sliderVol->setValue(Value);
        }
    }
    return 0;
}

void QWePlayer::on_sliderVol_valueChanged(int value)
{
    m_VolumeValue = ui->sliderVol->value();
    ui->lblVol->setText(QString::number(m_VolumeValue));
}

void QWePlayer::on_playprocess_clicked(qint64 pos)
{
    //seek位置
    PlaySeek(pos);
}
void QWePlayer::on_timer_Ticks()
{

}
void QWePlayer::on_timer_Ticks_fast()
{
    if (m_LongSeek > 0)
    {
        m_LongSeek--;
        if (m_LongSeek == 2)
        {
            int playTimeMS = m_Slider->value();

            if (m_Player)
            {
                ANPlayer_Inst_Seek(m_Player, playTimeMS, 0);
                //m_PlayBackHandle->Seek(playTimeMS);
                //PlayStart();
            }
        }
    }
}

int QWePlayer::GetPlayInfo(PLAYFILE_INFO& info)
{
    int curPlayIndex = GetCurPlayItemState(PLIST_STATE_PLAYING);
    if (curPlayIndex < 0)
    {
        info.filename = "";
        info.state = "stopped";
        info.list_indx = -1;
        info.total_length = (int)0;
        info.cur_time = (int)0;
        return 0;
    }
    QListWidgetItem *item = ui->listPlay->item(curPlayIndex);
    if (item)
    {
        int state;

        info.filename = item->data(PLIST_UDATA_FULLNAME).toString();
        info.list_indx = curPlayIndex;
        info.total_length = (int)m_TotalTime;
        info.cur_time = (int)m_CurPlayPts;

        if (m_Player == NULL)
        {
            info.state = "stopped";
        }
        else
        {
            state = ANPlayer_Inst_Get_State(m_Player);
            if (state == ANPLYAER_STATE_PAUSED)
            {
                info.state = "paused";
            }
            else
            {
                info.state = "playing";
            }
        }

        return 0;
    }

    return -1;
}
