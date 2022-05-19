#include "WePlayerSkin.h"
#include "anLogs.h"
#include <QDir>
#include <QMimeData>
#include <QScrollBar>
#include <QObject>
#include <QDomComment>
#include <QJsonArray>

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

WePlayerImage::WePlayerImage()
{
    m_loadDone = false;
    enabled = false;
}
void WePlayerImage::Load()
{

}
QString WePlayerImage::ClickUrl()
{
    return "";
}

WePlayerSkin::WePlayerSkin()
{
    m_borderSize = 0;
    m_toolbarMode = DISP_MODE_AUTO; //
    m_playlistMode = DISP_MODE_AUTO;

    //默认黑色背景
    m_bkColor = QColor(0, 0 , 0, 255);
    m_borderColor= QColor(0, 0 , 0, 255);
    m_bkImage = NULL;
    m_pauseImage = NULL;
}

WePlayerSkin::~WePlayerSkin()
{
    if (m_bkImage)
    {
        delete m_bkImage;
        m_bkImage = NULL;
    }

    if (m_pauseImage)
    {
        delete m_pauseImage;
        m_pauseImage = NULL;
    }
}

int WePlayerSkin::LoadFromFile(QString filename)
{
   return 0;
}
WePlayerImage* WePlayerSkin::loadImage(QJsonValue& node)
{
    WePlayerImage *playerImage = new WePlayerImage();

    if (playerImage)
    {
        QJsonObject bkImage = node.toObject();
        QJsonValue nodeImage;

        nodeImage = bkImage.value("enable");
        if (nodeImage.isBool())
        {
            playerImage->enabled = nodeImage.toBool();
        }

        nodeImage = bkImage.value("path");
        if (nodeImage.isString())
        {
            playerImage->path = nodeImage.toString();
        }

        nodeImage = bkImage.value("on_click_page");
        if (nodeImage.isString())
        {
            playerImage->clickUrl = nodeImage.toString();
        }

        nodeImage = bkImage.value("x");
        if (nodeImage.isDouble())
        {
            playerImage->rc.setX(nodeImage.toInt());
        }

        nodeImage = bkImage.value("y");
        if (nodeImage.isDouble())
        {
            playerImage->rc.setY(nodeImage.toInt());
        }

        nodeImage = bkImage.value("w");
        if (nodeImage.isDouble())
        {
            playerImage->rc.setWidth(nodeImage.toInt());
        }

        nodeImage = bkImage.value("h");
        if (nodeImage.isDouble())
        {
            playerImage->rc.setHeight(nodeImage.toInt());
        }
    }

    return playerImage;
}
int WePlayerSkin::LoadFromString(QString text)
{
    QByteArray byteData(text.toStdString().c_str());
    QJsonDocument parse_doucment = QJsonDocument::fromJson(byteData);
    QJsonObject RootNode = parse_doucment.object();

    if (RootNode.contains("bkcolor"))
    {
        QJsonValue bkNode = RootNode.value("bkcolor");
        QString colorText = bkNode.toString();
        m_bkColor = QColor(colorText);
    }

    if (RootNode.contains("playlist"))
    {
        QJsonValue node = RootNode.value("playlist");
        QString nodeTxt = node.toString();
        if (nodeTxt == "auto")
        {
            m_playlistMode = DISP_MODE_AUTO;
        }
        else if (nodeTxt == "show")
        {
            m_playlistMode = DISP_MODE_SHOW;
        }
        else if (nodeTxt == "hide")
        {
            m_playlistMode = DISP_MODE_HIDE;
        }
    }

    if (RootNode.contains("toolbar"))
    {
        QJsonValue node = RootNode.value("toolbar");
        QString nodeTxt = node.toString();
        if (nodeTxt == "auto")
        {
            m_toolbarMode = DISP_MODE_AUTO;
        }
        else if (nodeTxt == "show")
        {
            m_toolbarMode = DISP_MODE_SHOW;
        }
        else if (nodeTxt == "hide")
        {
            m_toolbarMode = DISP_MODE_HIDE;
        }
    }

    if (RootNode.contains("bkimage"))
    {
        QJsonValue node = RootNode.value("bkimage");
        if (node.isObject())
        {
            m_bkImage = loadImage(node);
        }
    }

    if (RootNode.contains("pauseimage"))
    {
        QJsonValue node = RootNode.value("pauseimage");
        if (node.isObject())
        {
            m_pauseImage = loadImage(node);
        }
    }

    if (RootNode.contains("osd"))
    {

    }

    return 0;
}

QColor WePlayerSkin::BackgroundColor()
{
    return m_bkColor;
}
QColor WePlayerSkin::BorderColor()
{
    return m_borderColor;
}
int WePlayerSkin::BorderSize()
{
    return m_borderSize;
}

WePlayerSkin::DISP_MODE WePlayerSkin::PlayListMode()
{
    return m_playlistMode;
}

WePlayerSkin::DISP_MODE WePlayerSkin::ToolbarMode()
{
    return m_toolbarMode;
}
