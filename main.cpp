#include "mainwindow.h"
#include <QApplication>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QTextCodec>
#include <QFile>
#include <QDesktopWidget>
#include "anIncludes.h"
#include "portaudio.h"
#include "anLogs.h"
#include "QWePlayer.h"

#ifdef AV_OS_WIN32
#ifdef QT_NO_DEBUG
#else
//#include "vld.h"
#endif
#endif

#ifdef AV_OS_WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

CWePlayerCfg* gPlayCfgData;

static int gScalerUI = 1.0;

void ANPlayerLog_FF_Callback(void* ptr, int level, const char* fmt, va_list vl)
{
    if (level <= AV_LOG_INFO)
    {
       va_list vl2;
       char line[1024]={0};
       static int print_prefix = 1;

       va_copy(vl2, vl);
       av_log_default_callback(ptr, level, fmt, vl);
       av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
       va_end(vl2);

       MessageOutput("%s",line);
    }
}

void InitScaledFactor()
{
    double rate = 1.0;

    QList<QScreen*> screens = QApplication::screens();
    if (screens.size() > 0)
    {
        QScreen* screen = screens[0];
        double dpi = screen->logicalDotsPerInch();
        rate = dpi / 96.0;

        if (rate < 1.1)
        {
            rate = 1.0;
        }
        else if (rate < 1.4)
        {
            rate = 1.25;
        }
        else if (rate < 1.6)
        {
            rate = 1.5;
        }
        else if (rate < 1.8)
        {
            rate = 1.75;
        }
        else
        {
            rate = 2.0;
        }
    }

    gScalerUI = rate;
}

int GetScaledSize(int size)
{
    return int (gScalerUI * size);
}

int GetScaledFont(int size)
{
    return int (size / gScalerUI);
}

double GetScaledFactor()
{
    return gScalerUI;
}

int AnLogCallback(int Type, const char* data, int dataLen)
{
    Type = Type;
    dataLen = dataLen;

    if (data)
    {
       qDebug() << data;
       //printf("%s", data);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    InitScaledFactor();
    gPlayCfgData = new CWePlayerCfg();
    Pa_Initialize();

    AvLogInit(AnLogCallback);

    QWePlayer::SystemInit(ANPlayerLog_FF_Callback);
#ifdef AV_OS_WIN32
    //初始化网络
    WSADATA wsa_data;
    WSAStartup(0x0201, &wsa_data);

#else
    QFont font;
    font.setPointSize(10);

    a.setFont(font);
#endif

    //作用域确保资源释放
    {
        MainWindow w;

        QDesktopWidget *desktop = QApplication::desktop();
        if (desktop)
        {
            QRect screenRect = desktop->screenGeometry();
            int w_width = 1280;
            int w_height = 720;

            w.setGeometry((screenRect.width() - w_width) / 2, (screenRect.height() - w_height) / 2, w_width, w_height);
        }

        if (argc >= 2)
        {
            for (int i = argc; i >= 1; i--)
            {
                QString filePath(argv[i]);
                QFile file(filePath);
                if(file.exists())
                {
                    w.AddToPlayList(filePath, i -1);
                }
            }
            w.AutoPlay();
        }
        w.show();
        a.exec();
    }

    QWePlayer::SystemTerminate();
    gPlayCfgData->SaveUserCfg();
    delete gPlayCfgData;

    Pa_Terminate();
    AvLogTerminate();
#ifdef AV_OS_WIN32
    WSACleanup();
#else

#endif

    return 0;
}
