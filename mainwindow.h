#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUdpSocket>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>
#include "QWePlayer.h"
#include "anIncludes.h"
#
#define APP_TALK_BUF_MAX  65535

#define WEPLAYER_VERSION  "2.0.2"

namespace Ui {
class MainWindow;
}

class WINDOW_MOVE
{
public:
    bool m_move;
    QPoint m_startPoint;
    QPoint m_windowPoint;
    qint64 m_pressTime;
};

#define min(a,b) ((a)<(b)? (a) :(b))
#define max(a,b) ((a)>(b)? (a) :(b))

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void StartTalker(QString addr, int port);
    void AutoPlay();
    void AddToPlayList(QString& file, int pos);
    void PlayListGetName(QString& file, int pos);

private:
    void ToggleFullScreen();
    void checkNewVersion();

protected:
  virtual void resizeEvent(QResizeEvent *);
  virtual bool eventFilter(QObject *,QEvent *);
  virtual void closeEvent( QCloseEvent * );

private slots:
    void on_btnClose_clicked();
    void on_timer_Ticks();

    void on_btnMin_clicked();
    void on_btnOption_clicked();
    void on_btnMax_clicked();

    void on_player_msg(int MsgCode, qint64 Param1, QString Param2, void* Param3);
    void on_talker_msg();
    void on_http_replay(QNetworkReply*);

private:
    Ui::MainWindow *ui;

    WINDOW_MOVE m_WindowMove;

    QPoint dragPosition;   //鼠标拖动的位置
    int    edgeMargin;     //鼠标检测的边缘距离
    QWidget * m_BorderTop;
    QWidget * m_BorderBottom;
    QWidget * m_BorderRight;
    QWidget * m_BorderLeft;

    QUdpSocket m_AppTalker;

    QNetworkAccessManager *m_NetManager;

    char* m_AppTalkBuf;

    bool m_enableNotify;
    QString m_NotifyAddr;
    int m_NotifyPort;

    QTimer *m_timer;
    int32_t m_ticks;
    int32_t m_mouseHideLeft;
};

#endif // MAINWINDOW_H
