#ifndef WEPLAYERMISC_H
#define WEPLAYERMISC_H

#include <QObject>
#include <QString>
#include <QDomComment>
#include <QJsonDocument>

class CCfgBase
{
public:
    CCfgBase();

    void SaveToFile(QString& szFile);
    void LoadFromFile(QString& szFile, QString RootName);

    //模板: /Default/xx/xxx /User/xx/xxx /Playlist/xx/xxx
    int GetNodeAttribute(QString Path, QString AttrName, QString& strResult);
    int SetNodeAttribute(QString Path, QString AttrName, QString AttrValue);

    int GetNodeText(QString Path, QString& strResult);
    int SetNodeText(QString Path, QString TextValue);

protected:
    QDomDocument m_Doc;
    QDomElement m_RootNode;
};

class CWePlayerCfg:public QObject
{
    Q_OBJECT
public:
    CWePlayerCfg();

    static QString GetCfgDir();

    QString GetNodeAttribute(QString Path, QString AttrName, QString strDefualt);
    int SetNodeAttribute(QString Path, QString AttrName, QString AttrValue);

    QString GetNodeText(QString Path, QString strDefualt);
    int SetNodeText(QString Path, QString TextValue);


    int SaveUserCfg();
    bool IsSupportVideoFile(QString& fileName);

private:


private:
    //程序路径
    QString m_ExePath;
    //用户数据文件
    QString m_UserDataPath;
    QString m_DefCfgFilePath;
    QString m_UserCfgFilePath;

    CCfgBase m_DefaultCfg;
    CCfgBase m_UserCfg;
};

#endif // WEPLAYERMISC_H
