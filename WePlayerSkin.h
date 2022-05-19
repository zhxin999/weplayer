#ifndef WEPLAYER_SKIN_H
#define WEPLAYER_SKIN_H
#include <QWidget>
#include <QList>
#include <QListWidget>
#include <QTimer>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QImage>

class WePlayerImage
{
public:
    WePlayerImage();
    void Load();
    QString ClickUrl();

    bool enabled;
    QRect rc;
    QImage image;
    QString path;
    QString name;
    QString clickUrl;
private:
    bool m_loadDone;
};

class WePlayerSkin
{
public:
    enum DISP_MODE{
        DISP_MODE_AUTO,
        DISP_MODE_SHOW,
        DISP_MODE_HIDE
    };

    WePlayerSkin();
    ~WePlayerSkin();

    int LoadFromFile(QString filename);
    int LoadFromString(QString text);

    QColor BackgroundColor();
    QColor BorderColor();
    int BorderSize();

    DISP_MODE PlayListMode();
    DISP_MODE ToolbarMode();
private:
    WePlayerImage* loadImage(QJsonValue& node);

private:
    QColor m_bkColor;
    QColor m_borderColor;
    int m_borderSize;
    DISP_MODE m_playlistMode;
    DISP_MODE m_toolbarMode;
    WePlayerImage* m_bkImage;
    WePlayerImage* m_pauseImage;
};

#endif
