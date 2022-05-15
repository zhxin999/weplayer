#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMouseEvent>
#include <QHostAddress>
#include "dlgoption.h"
#include "anLogs.h"

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#ifdef AV_OS_WIN32
#define TITLE_BAR_HEIGHT        32
#else
#define TITLE_BAR_HEIGHT        30
#endif


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
/*
#ifdef AV_OS_WIN32
    setWindowFlags(Qt::FramelessWindowHint|Qt::CustomizeWindowHint);
#else
    setWindowFlags(Qt::FramelessWindowHint);
#endif*/

    setWindowFlags(Qt::FramelessWindowHint|Qt::CustomizeWindowHint);

    //保证能移动，要鼠标穿透
    ui->lblTitle->setAttribute(Qt::WA_TranslucentBackground, true);
    ui->lblTitle->setAttribute(Qt::WA_TransparentForMouseEvents,true);

    m_BorderTop = new QWidget(this->centralWidget());
    m_BorderTop->setMouseTracking(true);
    m_BorderTop->installEventFilter(this);

    m_BorderBottom = new QWidget(this->centralWidget());
    m_BorderBottom->setMouseTracking(true);
    m_BorderBottom->installEventFilter(this);

    m_BorderRight = new QWidget(this->centralWidget());
    m_BorderRight->setMouseTracking(true);
    m_BorderRight->installEventFilter(this);

    m_BorderLeft = new QWidget(this->centralWidget());
    m_BorderLeft->setMouseTracking(true);
    m_BorderLeft->installEventFilter(this);

    m_WindowMove.m_move = false;

    this->installEventFilter(this);
    ui->frmTitleBar->installEventFilter(this);

    ui->videoWidget->installEventFilter(this);

    QString strCfgFile  = CWePlayerCfg::GetCfgDir() + QString("/player_one_cfg.xml");

    ui->videoWidget->LoadSetting(strCfgFile);
    connect(ui->videoWidget, SIGNAL(sgnlPlayerMsg(int, qint64, QString, void*)), this, SLOT(on_player_msg(int, qint64, QString, void*)));
#if 0
    ui->videoWidget->PlayListAdd("T:/VideoFiles/1080p.ts", nullptr, -1);
    ui->videoWidget->PlayListAdd("T:/VideoFiles/1.ts", nullptr, -1);
    ui->videoWidget->PlayListAdd("T:/VideoFiles/720.mp4", nullptr, -1);
    ui->videoWidget->PlayListAdd("T:/VideoFiles/src.mp3", nullptr, -1);
    ui->videoWidget->PlayListAdd("T:/VideoFiles/wang.mp3", nullptr, -1);
#else
    //ui->videoWidget->PlayListAdd("D:/Work/Git/AnShow/ffmpeg-3.3.2-win32-shared/bin/raw.mp3", nullptr, -1);
#endif
    //ui->videoWidget->PlayListSetCur(0);
    //ui->videoWidget->PlayStart();
#ifdef AV_OS_WIN32
    //ui->lblTitle->setStyleSheet("font: 10pt "微软雅黑"; color: rgb(255, 255, 255); ");
#else
    ui->lblTitle->setStyleSheet("font:10pt; color: rgb(255, 255, 255); ");
#endif

    ui->lblTitle->setText("视频播放器");

    edgeMargin = 4;

    m_AppTalkBuf = new char[APP_TALK_BUF_MAX];
    m_AppTalker.setParent(this);

    m_enableNotify = false;
    m_NotifyPort = 0;
    m_NotifyAddr = "127.0.0.1";

    ui->btnNewVesion->hide();

    m_NetManager = new QNetworkAccessManager(this);;
    connect(m_NetManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(on_http_replay(QNetworkReply*)));

#ifndef QT_DEBUG
    //debug版本不要查询升级，这个就是我自己调试的
    checkNewVersion();
#endif

    m_ticks = 0;
    //开启一个定时器
    m_timer=new QTimer(this);
    connect(m_timer,SIGNAL(timeout()),this,SLOT(on_timer_Ticks()));
    m_timer->start(1000);
}

MainWindow::~MainWindow()
{
    if (m_NetManager)
    {
        delete m_NetManager;
        m_NetManager = NULL;
    }

    if (m_AppTalkBuf)
    {
        delete m_AppTalkBuf;
        m_AppTalkBuf = NULL;
    }
    delete ui;
}

void MainWindow::on_timer_Ticks()
{
    m_ticks++;

    if (this->isFullScreen())
    {
        if (m_mouseHideLeft > 0)
        {
            m_mouseHideLeft--;
            if (m_mouseHideLeft == 0)
            {
                this->setCursor(Qt::BlankCursor);
            }
        }
    }

    if ((m_ticks % 60) == 0)
    {
#ifdef AV_OS_WIN32

#else
       //XScreenSaverSuspend();
#endif
    }
}

void MainWindow::StartTalker(QString addr, int port)
{
    m_AppTalker.bind(QHostAddress(addr), port);
    connect(&m_AppTalker, SIGNAL(readyRead()), this, SLOT(on_talker_msg()));
}

void MainWindow::AutoPlay()
{
    ui->videoWidget->PlayListSetCur(0);
    ui->videoWidget->PlayStart();
}

void MainWindow::AddToPlayList(QString& file, int pos)
{
    ui->videoWidget->PlayListAdd(file, nullptr, pos);
}

void MainWindow::PlayListGetName(QString& file, int pos)
{
    ui->videoWidget->PlayListGetName(pos, file);
}

void MainWindow::closeEvent( QCloseEvent * )
{
    QString path("");
    ui->videoWidget->PlayStop(false);
    ui->videoWidget->SaveSetting(path);
}

void MainWindow::resizeEvent(QResizeEvent *evt)
{
    int titlebar_height = TITLE_BAR_HEIGHT;

    if (ui->frmTitleBar->isHidden())
    {
        titlebar_height = 0;
        m_BorderLeft->hide();
        m_BorderRight->hide();
        m_BorderTop->hide();
        m_BorderBottom->hide();
    }
    else
    {
        m_BorderLeft->show();
        m_BorderRight->show();
        m_BorderTop->show();
        m_BorderBottom->show();
        m_BorderTop->setGeometry(0, 0, this->width(), edgeMargin);
        m_BorderLeft->setGeometry(0, edgeMargin, edgeMargin, this->height());
        m_BorderBottom->setGeometry(0, this->height() - edgeMargin, this->width(), edgeMargin);
        m_BorderRight->setGeometry(this->width() - edgeMargin, 0, edgeMargin, this->height());
        m_BorderRight->raise();
        m_BorderLeft->raise();
        m_BorderBottom->raise();
        m_BorderTop->raise();
    }


    //m_BorderTop->bri
    ui->frmTitleBar->setGeometry(0, 0, this->width(), titlebar_height);
    ui->videoWidget->setGeometry(0, titlebar_height, this->width(), this->height() - titlebar_height);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type()==QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = (QMouseEvent *)event;

        if((watched == ui->frmTitleBar) || 
            (watched == m_BorderTop) || (watched == m_BorderBottom) || (watched == m_BorderRight) || (watched == m_BorderLeft))
        {
            m_WindowMove.m_move = false;
            if ((this->isFullScreen() == false) && (this->isMaximized() == false))
            {
                m_WindowMove.m_move = true;
                //记录鼠标的世界坐标.
                m_WindowMove.m_startPoint = mouseEvent->globalPos();
                //记录窗体的世界坐标.
                m_WindowMove.m_windowPoint = this->frameGeometry().topLeft();

            }
        }
    }
    else if(event->type()==QEvent::MouseButtonRelease)
    {
        //QMouseEvent *mouseEvent = (QMouseEvent *)event;

        m_WindowMove.m_move = false;
    }
    else if(event->type()==QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = (QMouseEvent *)event;

        if (watched == ui->frmTitleBar)
        {
            if (m_WindowMove.m_move)
            {
                QPoint relativePos = mouseEvent->globalPos() - m_WindowMove.m_startPoint;
                this->move(m_WindowMove.m_windowPoint + relativePos );
            }
        }
        else if (watched == m_BorderTop) //SizeBDiagCursor
        {
            //鼠标移动
            if (m_WindowMove.m_move)
            {
                //QPoint relativePos = mouseEvent->globalPos() - m_WindowMove.m_startPoint;
                QRect rc = this->geometry();

                if (m_BorderTop->cursor().shape() == Qt::SizeFDiagCursor)
                {
                    rc.setTop(mouseEvent->globalPos().y());
                    rc.setLeft(mouseEvent->globalPos().x());
                }
                else if (m_BorderTop->cursor().shape() == Qt::SizeBDiagCursor)
                {
                    rc.setTop(mouseEvent->globalPos().y());
                    rc.setRight(mouseEvent->globalPos().x());
                }
                else
                {
                    rc.setTop(mouseEvent->globalPos().y());
                }

                this->setGeometry(rc);
            }
            else
            {
                if (mouseEvent->x() < 10)
                {
                    m_BorderTop->setCursor(Qt::SizeFDiagCursor);
                }
                else if (abs (this->width() - mouseEvent->x()) < 10)
                {
                    m_BorderTop->setCursor(Qt::SizeBDiagCursor);
                }
                else
                {
                    m_BorderTop->setCursor(Qt::SizeVerCursor);
                }
            }
        }
        else if (watched == m_BorderBottom)
        {
            //鼠标移动
            if (m_WindowMove.m_move)
            {
                //QPoint relativePos = mouseEvent->globalPos() - m_WindowMove.m_startPoint;
                QRect rc = this->geometry();

                if (m_BorderBottom->cursor().shape() == Qt::SizeFDiagCursor)
                {
                    rc.setBottom(mouseEvent->globalPos().y());
                    rc.setRight(mouseEvent->globalPos().x());
                }
                else if (m_BorderBottom->cursor().shape() == Qt::SizeBDiagCursor)
                {
                    rc.setBottom(mouseEvent->globalPos().y());
                    rc.setLeft(mouseEvent->globalPos().x());
                }
                else
                {
                    rc.setBottom(mouseEvent->globalPos().y());
                }
                this->setGeometry(rc);
            }
            else
            {
                if (mouseEvent->x() < 10)
                {
                    m_BorderBottom->setCursor(Qt::SizeBDiagCursor);
                }
                else if (abs (this->width() - mouseEvent->x()) < 10)
                {
                    m_BorderBottom->setCursor(Qt::SizeFDiagCursor);
                }
                else
                {
                    m_BorderBottom->setCursor(Qt::SizeVerCursor);
                }
            }
        }
        else if (watched == m_BorderRight)
        {
            //鼠标移动
            if (m_WindowMove.m_move)
            {
                QRect rc = this->geometry();

                if (m_BorderRight->cursor().shape() == Qt::SizeFDiagCursor)
                {
                    rc.setBottom(mouseEvent->globalPos().y());
                    rc.setRight(mouseEvent->globalPos().x());
                }
                else if (m_BorderRight->cursor().shape() == Qt::SizeBDiagCursor)
                {
                    rc.setTop(mouseEvent->globalPos().y());
                    rc.setRight(mouseEvent->globalPos().x());
                }
                else
                {
                    rc.setRight(mouseEvent->globalPos().x());
                }

                this->setGeometry(rc);
            }
            else
            {
                if (mouseEvent->y() < 10)
                {
                    m_BorderRight->setCursor(Qt::SizeBDiagCursor);
                }
                else if (abs (this->height() - mouseEvent->y()) < 10)
                {
                    m_BorderRight->setCursor(Qt::SizeFDiagCursor);
                }
                else
                {
                    m_BorderRight->setCursor(Qt::SizeHorCursor);
                }
            }
        }
        else if (watched == m_BorderLeft)
        {
            //鼠标移动
            if (m_WindowMove.m_move)
            {
                QPoint relativePos = mouseEvent->globalPos() - m_WindowMove.m_startPoint;
                QRect rc = this->geometry();

                if (m_BorderLeft->cursor().shape() == Qt::SizeFDiagCursor)
                {
                    rc.setTop(mouseEvent->globalPos().y());
                    rc.setLeft(mouseEvent->globalPos().x());
                }
                else if (m_BorderLeft->cursor().shape() == Qt::SizeBDiagCursor)
                {
                    rc.setBottom(mouseEvent->globalPos().y());
                    rc.setLeft(mouseEvent->globalPos().x());
                }
                else
                {
                    rc.setLeft(mouseEvent->globalPos().x());
                }

                this->setGeometry(rc);
            }
            else
            {
                if (mouseEvent->y() < 10)
                {
                    m_BorderLeft->setCursor(Qt::SizeFDiagCursor);
                }
                else if (abs (this->height() - mouseEvent->y()) < 10)
                {
                    m_BorderLeft->setCursor(Qt::SizeBDiagCursor);
                }
                else
                {
                    m_BorderLeft->setCursor(Qt::SizeHorCursor);
                }
            }
        }

        if (this->isFullScreen())
        {
            if (m_mouseHideLeft <= 0)
            {
                this->setCursor(Qt::ArrowCursor);
            }
        }
        m_mouseHideLeft = 15;
    }
    else if(event->type()==QEvent::MouseButtonDblClick)
    {
        if (watched == ui->frmTitleBar)
        {
            if (this->isMaximized())
            {
                this->showNormal();
                ui->btnMax->setStyleSheet("QPushButton#btnMax{border-image:url(:/resource/max.png);}"
                   "QPushButton#btnMax:hover{border-image:url(:/resource/max_hover.png);}"
                   "QPushButton#btnMax:pressed{border-image:url(:/resource/max.png);}");
            }
            else
            {
                this->showMaximized();
                ui->btnMax->setStyleSheet("QPushButton#btnMax{border-image:url(:/resource/restore.png);}"
                   "QPushButton#btnMax:hover{border-image:url(:/resource/restore_hover.png);}"
                   "QPushButton#btnMax:pressed{border-image:url(:/resource/restore.png);}");
            }
        }
    }

    return QWidget::eventFilter(watched,event);
}

void MainWindow::on_btnClose_clicked()
{
    this->close();

}

void MainWindow::on_btnMin_clicked()
{
    this->showMinimized();
}

void MainWindow::on_btnMax_clicked()
{
    if (this->isMaximized())
    {
        this->showNormal();
        ui->btnMax->setStyleSheet("QPushButton#btnMax{border-image:url(:/resource/max.png);}"
           "QPushButton#btnMax:hover{border-image:url(:/resource/max_hover.png);}"
           "QPushButton#btnMax:pressed{border-image:url(:/resource/max.png);}");
    }
    else
    {
        this->showMaximized();
        ui->btnMax->setStyleSheet("QPushButton#btnMax{border-image:url(:/resource/restore.png);}"
           "QPushButton#btnMax:hover{border-image:url(:/resource/restore_hover.png);}"
           "QPushButton#btnMax:pressed{border-image:url(:/resource/restore.png);}");
    }
}
void MainWindow::ToggleFullScreen()
{
    if (this->isFullScreen())
    {
        ui->frmTitleBar->show();
        this->showNormal();
        ui->videoWidget->ToggleFullScreen(false);
    }
    else
    {
        ui->frmTitleBar->hide();
        this->showFullScreen();
        ui->videoWidget->ToggleFullScreen(true);

        m_mouseHideLeft = 15;
    }

    //强制重新布局，要不然在扩展屏幕上会有问题
    resizeEvent(NULL);
}
void MainWindow::on_player_msg(int MsgCode, qint64 Param1, QString Param2, void* Param3)
{
    //
    if (MsgCode == PLAYER_MSG_VIDEO_DBCLICK)
    {
        ToggleFullScreen();
    }
    else if (MsgCode == PLAYER_MSG_FULLSCREEN_TOGGLE)
    {
        ToggleFullScreen();
    }
    else if (MsgCode == PLAYER_MSG_OPENFILE_DONE)
    {
        int indx = Param2.lastIndexOf("/");
        QString shortName;
        if (indx < 0)
            shortName = Param2;
        else
            shortName = Param2.mid(indx + 1);

        //设置当前播放的文件名字
        if (m_enableNotify == true)
        {
            QVariantHash replay;

            replay.insert("notify", "playstart");
            replay.insert("instance", 0);
            replay.insert("filaname", Param2);

            QJsonObject rootObj = QJsonObject::fromVariantHash(replay);
            QJsonDocument document;
            document.setObject(rootObj);
            QByteArray byte_array = document.toJson(QJsonDocument::Compact);
            
            m_AppTalker.writeDatagram(byte_array.data(), byte_array.length(), QHostAddress(m_NotifyAddr), m_NotifyPort);
        }
        ui->lblTitle->setText(shortName);
    }
    else if (MsgCode == PLAYER_MSG_PLAYFILE_DONE)
    {
        //提示当前播放完了
        if (m_enableNotify == true)
        {
            QVariantHash replay;

            replay.insert("notify", "playstop");
            replay.insert("instance", 0);

            QJsonObject rootObj = QJsonObject::fromVariantHash(replay);
            QJsonDocument document;
            document.setObject(rootObj);
            QByteArray byte_array = document.toJson(QJsonDocument::Compact);

            m_AppTalker.writeDatagram(byte_array.data(), byte_array.length(), QHostAddress(m_NotifyAddr), m_NotifyPort);
        }

        ui->lblTitle->setText("视频播放器");
    }
    else if (MsgCode == PLAYER_MSG_PLAYPAUSE)
    {
        //设置当前播放的文件名字
        if (m_enableNotify == true)
        {
            QVariantHash replay;

            replay.insert("notify", "playpause");
            replay.insert("instance", 0);

            QJsonObject rootObj = QJsonObject::fromVariantHash(replay);
            QJsonDocument document;
            document.setObject(rootObj);
            QByteArray byte_array = document.toJson(QJsonDocument::Compact);

            m_AppTalker.writeDatagram(byte_array.data(), byte_array.length(), QHostAddress(m_NotifyAddr), m_NotifyPort);
        }
    }
    else if (MsgCode == PLAYER_MSG_PLAYRESUME)
    {
        if (m_enableNotify == true)
        {
            QVariantHash replay;

            replay.insert("notify", "playresume");
            replay.insert("instance", 0);

            QJsonObject rootObj = QJsonObject::fromVariantHash(replay);
            QJsonDocument document;
            document.setObject(rootObj);
            QByteArray byte_array = document.toJson(QJsonDocument::Compact);

            m_AppTalker.writeDatagram(byte_array.data(), byte_array.length(), QHostAddress(m_NotifyAddr), m_NotifyPort);
        }
    }
}

void MainWindow::on_btnOption_clicked()
{
    DlgOption dlgOption(NULL);
    int render_mode;
    bool keep_aspect = ui->videoWidget->GetKeepAspect();

    render_mode = ui->videoWidget->GetRenderMode();
    dlgOption.SetHardAccelMode(render_mode);
    dlgOption.SetFormatExt(gPlayCfgData->GetNodeAttribute("DefaultConfig/Player/SupportFile", "extlist", ""));

    dlgOption.SetKeepAspect(keep_aspect);
    dlgOption.SetUserFormat(gPlayCfgData->GetNodeAttribute("UserConfig/Format", "extlist", ""));

    dlgOption.move(this->x() + (this->width() - dlgOption.width()) / 2, this->y() + (this->height() - dlgOption.height()) / 2);
    if (dlgOption.exec() == 1)
    {
        int mode = dlgOption.GetHardAccelMode();

        QString usrExtList = dlgOption.GetUserFormat();

        gPlayCfgData->SetNodeAttribute("UserConfig/Format", "extlist", usrExtList);

        ui->videoWidget->SetKeepAspect(dlgOption.GetKeepAspect());

        ui->videoWidget->SetRenderModeCfg(mode);
        QString path("");
        ui->videoWidget->SaveSetting(path);
    }

}

//这个是和APP程序的通信消息
void MainWindow::on_talker_msg()
{
    int read_len;
    char szMsg[256];

    //localhost最多是64K
    if (m_AppTalkBuf == NULL)
    {
        return;
    }

    while (m_AppTalker.hasPendingDatagrams())
    {
        QHostAddress fromHost;
        quint16 fromPort = 0;

        read_len = m_AppTalker.readDatagram(m_AppTalkBuf, APP_TALK_BUF_MAX - 1, &fromHost, &fromPort);
        if (read_len <= 0)
        {
            break;
        }
        m_AppTalkBuf[read_len] = '\0';

        QByteArray byteData(m_AppTalkBuf);

        QJsonDocument parse_doucment = QJsonDocument::fromJson(byteData);
        QJsonObject RootNode = parse_doucment.object();

        if (RootNode.contains("ask"))
        {
            QJsonValue askNode = RootNode.value("ask");
            QString askValue = askNode.toString();
            QJsonValue instanceNode = RootNode.value("instance");
            int playerinstance = -1;

            //暂时好像不关心这个参数
            if (!instanceNode.isDouble())
            {
                playerinstance = askValue.toInt();
            }

            if (askValue == QString("fileadd"))
            {//添加文件到播放列表
                QJsonValue pathNode = RootNode.value("path");
                QJsonValue indexNode = RootNode.value("index");
                int fileindex = -1;

                if (indexNode.isDouble())
                {
                    fileindex = indexNode.toInt();
                }

                if (pathNode.isString())
                {
                    //添加
                    ui->videoWidget->PlayListAdd(pathNode.toString(), nullptr, fileindex);
                    strcpy(szMsg, "{\"answer\":\"fileadd_rsp\", \"code\":0, \"msg\":\"OK\"}");
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"fileadd_rsp\", \"code\":-1, \"msg\":\"invalid format\"}");
                }

                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);

            }
            else if (askValue == QString("filedelete"))
            {//从列表里面删除文件，删除的话只看索引
                QJsonValue indexNode = RootNode.value("index");
                int fileindex = -1;

                if (indexNode.isDouble())
                {
                    fileindex = indexNode.toInt();
                }

                if (fileindex >= 0)
                {
                    ui->videoWidget->PlayListDelete(fileindex);
                    strcpy(szMsg, "{\"answer\":\"filedelete_rsp\", \"code\":0, \"msg\":\"OK\"}");
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"filedelete_rsp\", \"code\":-1, \"msg\":\"invalid index\"}");
                }

                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("listclear"))
            {//清楚所有的播放文件
                ui->videoWidget->PlayListClean();
                strcpy(szMsg, "{\"answer\":\"filedelete_rsp\", \"code\":0, \"msg\":\"OK\"}");
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("listget"))
            {//获取当前播放列表，里面会有一个最大长度限制，不超过60K
                QVariantHash replay;
                QVariantList list;

                replay.insert("answer", "listget_rsp");
                replay.insert("code", 0);

                ui->videoWidget->PlayListGet(list);

                replay.insert("list", list);

                QJsonObject rootObj = QJsonObject::fromVariantHash(replay);
                QJsonDocument document;
                document.setObject(rootObj);
                QByteArray byte_array = document.toJson(QJsonDocument::Compact);
                m_AppTalker.writeDatagram(byte_array.data(), byte_array.length(), fromHost, fromPort);
            }
            else if (askValue == QString("fileopen"))
            {//打开指定的文件播放
                QJsonValue indexNode = RootNode.value("index");
                int fileindex = -1;

                if (indexNode.isDouble())
                {
                    fileindex = indexNode.toInt();
                }

                if (fileindex >= 0)
                {
                    ui->videoWidget->PlayListSetCur(fileindex);
                    strcpy(szMsg, "{\"answer\":\"fileopen_rsp\", \"code\":0, \"msg\":\"OK\"}");
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"fileopen_rsp\", \"code\":-1, \"msg\":\"invalid index\"}");
                }

                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("playstart"))
            {//开始播放
                //开始播放，就相当于点击开始按钮
                int retcode = -1;

                retcode = ui->videoWidget->PlayStart();

                if (retcode == 0)
                {
                    strcpy(szMsg, "{\"answer\":\"playstart_rsp\", \"code\":0, \"msg\":\"OK\"}");
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"playstartn_rsp\", \"code\":-1, \"msg\":\"already start open\"}");
                }

                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("playpause"))
            {//暂停，继续按钮
                int retcode = -1;

                retcode = ui->videoWidget->PlayPause();

                if (retcode == 0)
                {
                    strcpy(szMsg, "{\"answer\":\"playstart_rsp\", \"code\":0, \"msg\":\"OK\"}");
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"playstartn_rsp\", \"code\":-1, \"msg\":\"not playing\"}");
                }
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("playstop"))
            {//停止播放
                int retcode = -1;

                retcode = ui->videoWidget->PlayStop(false);

                if (retcode == 0)
                {
                    strcpy(szMsg, "{\"answer\":\"playstop_rsp\", \"code\":0, \"msg\":\"OK\"}");
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"playstop_rsp\", \"code\":-1, \"msg\":\"error\"}");
                }
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("seek"))
            {//跳转
                QJsonValue seekNode = RootNode.value("seek");
                if (seekNode.isDouble())
                {
                    int seek = seekNode.toInt();
                    if (seek < 0)
                    {
                        strcpy(szMsg, "{\"answer\":\"seek_rsp\", \"code\":-1, \"msg\":\"invalid seek position\"}");
                    }
                    else
                    {
                        ui->videoWidget->PlaySeek(seek);
                        strcpy(szMsg, "{\"answer\":\"seek_rsp\", \"code\":0, \"msg\":\"ok\"}");
                    }
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"seek_rsp\", \"code\":-1, \"msg\":\"no seek position\"}");
                }
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("movewindow"))
            {//移动窗口
                QJsonValue fullscreenNode = RootNode.value("fullscreen");
                QJsonValue positionNode = RootNode.value("pos");

                if (fullscreenNode.isBool())
                {
                    if (fullscreenNode.toBool() == true)
                    {
                        //进入全屏
                        if (!this->isFullScreen())
                        {
                            this->ToggleFullScreen();
                            strcpy(szMsg, "{\"answer\":\"movewindow_rsp\", \"code\":0, \"msg\":\"ok\"}");
                        }
                        else
                        {
                            strcpy(szMsg, "{\"answer\":\"movewindow_rsp\", \"code\":0, \"msg\":\"alread full screen\"}");
                        }
                    }
                    else
                    {
                        //移动窗口
                        if (this->isFullScreen())
                        {
                            this->ToggleFullScreen();
                            strcpy(szMsg, "{\"answer\":\"movewindow_rsp\", \"code\":0, \"msg\":\"quit full screen\"}");
                        }
                        else
                        {
                            //移动窗口
                            if (positionNode.isString())
                            {
                                int x, y, w, h;
                                if (sscanf(positionNode.toString().toStdString().c_str(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
                                {
                                    this->setGeometry(x, y, w, h);
                                    strcpy(szMsg, "{\"answer\":\"movewindow_rsp\", \"code\":0, \"msg\":\"ok\"}");
                                }
                                else
                                {
                                    strcpy(szMsg, "{\"answer\":\"movewindow_rsp\", \"code\":-1, \"msg\":\"invalid position format\"}");
                                }
                            }
                            else
                            {
                                strcpy(szMsg, "{\"answer\":\"movewindow_rsp\", \"code\":-1, \"msg\":\"invalid position\"}");
                            }
                        }
                    }
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"movewindow_rsp\", \"code\":-1, \"msg\":\"invalid fullscreen node\"}");
                }
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("setvolume"))
            {//设置声音的接口
                QJsonValue muteNode = RootNode.value("mute");
                QJsonValue valueNode = RootNode.value("value");
                if (valueNode.isDouble() && muteNode.isBool())
                {
                    int volume = valueNode.toInt();
                    if ((volume < 0) ||( volume > 100))
                    {
                        strcpy(szMsg, "{\"answer\":\"setvolume_rsp\", \"code\":-1, \"msg\":\"invalid volume value\"}");
                    }
                    else
                    {
                        ui->videoWidget->SetVolume(muteNode.toBool(), volume);
                        strcpy(szMsg, "{\"answer\":\"setvolume_rsp\", \"code\":0, \"msg\":\"ok\"}");
                    }
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"setvolume_rsp\", \"code\":-1, \"msg\":\"no volume value\"}");
                }
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("setnotify"))
            {//允许设置播放提醒信息的位置，设置播放器的提醒信息
                QJsonValue modeNode = RootNode.value("mode");
                QJsonValue portNode = RootNode.value("port");
                QJsonValue addrNode = RootNode.value("addr");
                if (modeNode.isDouble())
                {
                    int mode = modeNode.toInt();
                    if (mode == 0)
                    {
                        m_enableNotify = false;
                        strcpy(szMsg, "{\"answer\":\"setvolume_rsp\", \"code\":0, \"msg\":\"ok\"}");
                    }
                    else if (mode == 1)
                    {
                        m_enableNotify = true;
                        if (portNode.isDouble() && addrNode.isString())
                        {
                            if (portNode.toInt() == 0)
                            {
                                m_NotifyPort = fromPort;
                                m_NotifyAddr = fromHost.toString();
                            }
                            else
                            {
                                m_NotifyPort = portNode.toInt();
                                m_NotifyAddr = addrNode.toString();
                            }
                            strcpy(szMsg, "{\"answer\":\"setnotify_rsp\", \"code\":0, \"msg\":\"ok\"}");
                        }
                        else
                        {
                            strcpy(szMsg, "{\"answer\":\"setnotify_rsp\", \"code\":-1, \"msg\":\"invalid port or addr\"}");
                        }
                    }
                    else
                    {
                        strcpy(szMsg, "{\"answer\":\"setnotify_rsp\", \"code\":-1, \"msg\":\"invalid mode\"}");
                    }
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"setnotify_rsp\", \"code\":-1, \"msg\":\"no mode value\"}");
                }
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }
            else if (askValue == QString("playinfo"))
            {//当前的播放信息
                PLAYFILE_INFO info;
                if (ui->videoWidget->GetPlayInfo(info) == 0)
                {
                    //返回执行这些界面信息
                    QVariantHash replay;
                    QVariantHash playinfo;

                    replay.insert("answer", "playinfo_rsp");
                    replay.insert("code", 0);

                    playinfo.insert("filename", info.filename);
                    playinfo.insert("state", info.state);
                    playinfo.insert("list_indx", info.list_indx);
                    playinfo.insert("total", info.total_length);
                    playinfo.insert("current", info.cur_time);

                    replay.insert("info", playinfo);

                    QJsonObject rootObj = QJsonObject::fromVariantHash(replay);
                    QJsonDocument document;
                    document.setObject(rootObj);
                    QByteArray byte_array = document.toJson(QJsonDocument::Compact);

                    m_AppTalker.writeDatagram(byte_array.data(), byte_array.length(), fromHost, fromPort);
                }
                else
                {
                    strcpy(szMsg, "{\"answer\":\"playinfo_rsp\", \"code\":-1, \"msg\":\"failed\"}");
                    m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
                }
            }
            else
            {
                strcpy(szMsg, "{\"error\":\"error\", \"code\":-1, \"msg\":\"not support\"}");
                m_AppTalker.writeDatagram(szMsg, strlen(szMsg), fromHost, fromPort);
            }

        }
        else
        {
            MessageError("[%s:%d] error message format\n", __FUNCTION__, __LINE__);
            //qDebug() << "error format>:" << RootNode.isEmpty();
        }
    }
}

//检查有不有新版本
void MainWindow::checkNewVersion()
{
    QNetworkRequest request;
    QString params;
    QString url = "http://www.ucosoft.cn/api/v2/weplayer/update_check?";

#if defined AV_OS_WIN32
    params += "os=win32";
#elif defined _UOS_X86_64
    params += "os=uos";
#endif

    //cpu架构
    params += "&arch=";
    params += "x86";

    //版本号
    params += "&version=";
    params += WEPLAYER_VERSION;

    QString str_uuid = gPlayCfgData->GetNodeAttribute("UserConfig/UUID", "value", "");
    if (str_uuid.length() > 0)
    {
        params += "&uuid=";
        params += str_uuid;
    }

    url.append(params);

    request.setUrl(QUrl(url));
    QNetworkReply* reply = m_NetManager->get(request);
}

void MainWindow::on_http_replay(QNetworkReply *reply)
{
    /*
     * 返回格斯 {"code":0, "msg":"ok","data":{"url":"xxx", "version":"222","msg":"有新版本，请到应用商店下载"}}
    */
    if(reply && reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        //QByteArray byteData(data.toStdString());
        QJsonDocument parse_doucment = QJsonDocument::fromJson(data);
        QJsonObject RootNode = parse_doucment.object();

        if (RootNode.contains("code"))
        {
            QJsonValue codeNode = RootNode.value("code");
            int codeValue = codeNode.toInt();
            QJsonValue dataNode = RootNode.value("data");

            if ((codeValue == 0) && dataNode.isObject())
            {
                QJsonObject dataObj = dataNode.toObject();
                //QJsonValue urlNode = dataObj.value("url");
                //QJsonValue versionNode = dataObj.value("version");
                QJsonValue msgNode = dataObj.value("msg");
                if (msgNode.isString())
                {//开始显示有更新的图标
                    ui->btnNewVesion->show();
                    ui->btnNewVesion->setToolTip(msgNode.toString());
                }

            }
        }
        else
        {
            MessageError("[%s:%d] error response format:%s\n", __FUNCTION__, __LINE__, data.toStdString().c_str());
        }
    }
    else
    {
        //qDebug() << reply->errorString();
        MessageError("[%s:%d] Failed to query new version:%s\n", __FUNCTION__, __LINE__, reply->errorString().toStdString().c_str());
    }

    reply->close();
}
