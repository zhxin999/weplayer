#include "mainwindow.h"
#include <QApplication>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QTextCodec>
#include <QFile>
#include <QDesktopWidget>
#include <QUuid>
#include "anIncludes.h"
#include "portaudio.h"
#include "anLogs.h"
#include "QWePlayer.h"


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
#ifdef AV_OS_WIN32
       qDebug() << data;
#else
       printf("%s", data);
       fflush(stdout);
#endif
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
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
#else
    QFont font;
    font.setPointSize(10);

    a.setFont(font);
#endif

    {
        QString str_uuid = gPlayCfgData->GetNodeAttribute("UserConfig/UUID", "value", "");
        if (str_uuid.length() < 1)
        {
            QUuid uuid = QUuid::createUuid();
            QString strUUId = uuid.toString().remove("{").remove("}").toUpper();
            gPlayCfgData->SetNodeAttribute("UserConfig/UUID", "value", strUUId);
            gPlayCfgData->SaveUserCfg();
        }

    }

    //作用域确保资源释放
    {
        MainWindow w;
        QString talkerBind = "127.0.0.1";
        int talkerPort = 22223;
        bool enableTalker = true;

        QDesktopWidget *desktop = QApplication::desktop();
        if (desktop)
        {
            QRect screenRect = desktop->screenGeometry();
            int w_width = screenRect.width() / 2;
            int w_height = screenRect.height() / 2;

            w.setGeometry((screenRect.width() - w_width) / 2, (screenRect.height() - w_height) / 2, w_width, w_height);
        }

        if (argc >= 2)
        {
            QString filePath(argv[1]);
            QFile file(filePath);
            if(file.exists())
            {
                QString filename;

                w.PlayListGetName(filename, 0);

                if (filename != filePath)
                {
                    w.AddToPlayList(filePath, 0);
                }

                w.AutoPlay();
            }

            //开始解析参数
            for (int i = 2; i < argc; i++)
            {
                if (strncmp(argv[i], "-P=", 3) == 0)
                {//记录下端口
                    talkerPort = strtol(argv[i] + 3, NULL, 0);
                }
                else if (strncmp(argv[i], "-A=", 3) == 0)
                {//记录下地址
                    talkerBind = argv[i] + 3;
                }
            }
        }

        if (enableTalker)
        {
            w.StartTalker(talkerBind, talkerPort);
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
    SetThreadExecutionState(ES_CONTINUOUS);
#else

#endif

    return 0;
}
