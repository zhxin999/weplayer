#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUdpSocket>
#include "QWePlayer.h"
#include "anIncludes.h"
#
#define APP_TALK_BUF_MAX  65535

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

    void AutoPlay();
    void AddToPlayList(QString& file, int pos);

#ifdef AV_OS_WIN32
    //bool nativeEvent(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;
#endif
private:
    void ToggleFullScreen();


protected:
  virtual void resizeEvent(QResizeEvent *);
  virtual bool eventFilter(QObject *,QEvent *);
  virtual void closeEvent( QCloseEvent * );

private slots:
    void on_btnClose_clicked();

    void on_btnMin_clicked();
    void on_btnOption_clicked();
    void on_btnMax_clicked();

    void on_player_msg(int MsgCode, qint64 Param1, QString Param2, void* Param3);
    void on_talker_msg();

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

    char* m_AppTalkBuf;

    bool m_enableNotify;
    QString m_NotifyAddr;
    int m_NotifyPort;
};

#endif // MAINWINDOW_H
