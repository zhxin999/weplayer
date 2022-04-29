#include "CWePlayerCfg.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QMessageBox>

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

CCfgBase::CCfgBase()
{

}

void CCfgBase::SaveToFile(QString& szFile)
{
    QFile file(szFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    QTextStream out(&file);
    out.setCodec("UTF-8");
    m_Doc.save(out, 4, QDomNode::EncodingFromTextStream);
    file.close();
}

void CCfgBase::LoadFromFile(QString& szFile, QString RootName)
{
    QFile file(szFile);
    QString error;
    int row = 0, column = 0;

    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        goto CreateNode;
    }

    if(!m_Doc.setContent(&file, false, &error, &row, &column))
    {
        goto CreateNode;
    }

    if(m_Doc.isNull())
    {
        goto CreateNode;
    }

    m_RootNode = m_Doc.documentElement();

    if (m_RootNode.tagName() != RootName)
    {
        QMessageBox::information(NULL, QString("默认配置文件格式不匹配"), QString("错误"));
        m_Doc.clear();
        m_RootNode.clear();
        goto CreateNode;
    }
    else
    {

    }

    return;

CreateNode:
    QDomProcessingInstruction instruction = m_Doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    m_Doc.appendChild(instruction);

    m_RootNode = m_Doc.createElement(RootName);//创建根节点
    m_Doc.appendChild(m_RootNode);//添加根节点
}

int CCfgBase::GetNodeAttribute(QString Path, QString AttrName, QString& strResult)
{
    if (m_RootNode.isNull())
        return -1;

    QStringList myList = Path.split('/');

    int size = myList.size();
    int idx = 1;

    if (size > 0)
    {
        QDomElement element = m_RootNode.firstChildElement(myList[idx]);
        while(!element.isNull())
        {
            if (idx == size - 1)
            {
                strResult =  element.attribute(AttrName);

                if (strResult.isNull())
                {
                    return -1;
                }
                else
                    return 0;
            }

            idx++;
            element = element.firstChildElement(myList[idx]);
        }
    }

    return -1;
}

int CCfgBase::SetNodeAttribute(QString Path, QString AttrName, QString AttrValue)
{
    if (m_RootNode.isNull())
        return -1;

    QStringList myList = Path.split('/');

    int size = myList.size();

    QDomElement curNode = m_RootNode;
    for (int i = 1; i < size; i++)
    {
        QDomElement element = curNode.firstChildElement(myList[i]);
        if (element.isNull())
        {
            element = m_Doc.createElement(myList[i]);
            curNode.appendChild(element);
        }

        if (i == size - 1)
        {
            //设置属性
            element.setAttribute(AttrName, AttrValue);
            break;
        }

        curNode = element;
    }

    return 0;
}

int CCfgBase::GetNodeText(QString Path, QString& strResult)
{
    if (m_RootNode.isNull())
        return -1;

    QStringList myList = Path.split('/');

    int size = myList.size();
    int idx = 1;

    if (size > 0)
    {
        QDomElement element = m_RootNode.firstChildElement(myList[idx]);
        while(!element.isNull())
        {
            if (idx == size - 1)
            {
                strResult =  element.text();

                if (strResult.isNull())
                {
                    return -1;
                }
                else
                    return 0;
            }

            idx++;
            element = element.firstChildElement(myList[idx]);
        }
    }

    return -1;
}
int CCfgBase::SetNodeText(QString Path, QString TextValue)
{
    if (m_RootNode.isNull())
        return -1;

    QStringList myList = Path.split('/');

    int size = myList.size();

    QDomElement curNode = m_RootNode;
    for (int i = 1; i < size; i++)
    {
        QDomElement element = curNode.firstChildElement(myList[i]);
        if (element.isNull())
        {
            element = m_Doc.createElement(myList[i]);
            curNode.appendChild(element);
        }

        if (i == size - 1)
        {
            QDomText text;
            text=m_Doc.createTextNode(TextValue);
            element.removeChild(element.firstChild());
            element.appendChild(text);
            break;
        }

        curNode = element;
    }

    return 0;
}

CWePlayerCfg::CWePlayerCfg()
{
    m_ExePath = QCoreApplication::applicationDirPath();
    m_DefCfgFilePath = m_ExePath + QString("/default.xml");
    m_UserDataPath = GetCfgDir();
    m_UserCfgFilePath = m_UserDataPath + QString("/userconfig.xml");

    QDir dirSubtext(m_UserDataPath);
    if (!dirSubtext.exists())
    {
        dirSubtext.mkdir(m_UserDataPath);
    }

    //加载配置
    m_UserCfg.LoadFromFile(m_UserCfgFilePath, "UserConfig");
    m_DefaultCfg.LoadFromFile(m_DefCfgFilePath, "DefaultConfig");
}
QString CWePlayerCfg::GetCfgDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString CWePlayerCfg::GetNodeAttribute(QString Path, QString AttrName, QString strDefualt)
{
    QStringList myList = Path.split('/');

    int size = myList.size();
    if (size > 0)
    {
        if (myList[0] == QString("UserConfig"))
        {
            QString strResult;
            if (m_UserCfg.GetNodeAttribute(Path, AttrName, strResult) == 0)
            {
                return strResult;
            }
        }
        else if (myList[0] == QString("DefaultConfig"))
        {
            QString strResult;
            if (m_DefaultCfg.GetNodeAttribute(Path, AttrName, strResult) == 0)
            {
                return strResult;
            }
        }

    }

    return strDefualt;
}

int CWePlayerCfg::SetNodeAttribute(QString Path, QString AttrName, QString AttrValue)
{
    QStringList myList = Path.split('/');

    int size = myList.size();
    if (size > 0)
    {
        if (myList[0] == QString("UserConfig"))
        {
            return m_UserCfg.SetNodeAttribute(Path, AttrName, AttrValue);
        }

    }

    return -1;
}

QString CWePlayerCfg::GetNodeText(QString Path, QString strDefualt)
{
    QStringList myList = Path.split('/');

    int size = myList.size();
    if (size > 0)
    {
        if (myList[0] == QString("UserConfig"))
        {
            QString strResult;
            if (m_UserCfg.GetNodeText(Path, strResult) == 0)
            {
                return strResult;
            }
        }
        else if (myList[0] == QString("DefaultConfig"))
        {
            QString strResult;
            if (m_DefaultCfg.GetNodeText(Path, strResult) == 0)
            {
                return strResult;
            }
        }

    }

    return strDefualt;
}
int CWePlayerCfg::SetNodeText(QString Path, QString TextValue)
{
    QStringList myList = Path.split('/');

    int size = myList.size();
    if (size > 0)
    {
        if (myList[0] == QString("UserConfig"))
        {
            return m_UserCfg.SetNodeText(Path, TextValue);
        }

    }

    return -1;
}

int CWePlayerCfg::SaveUserCfg()
{
    m_UserCfg.SaveToFile(m_UserCfgFilePath);
    return 0;
}

bool CWePlayerCfg::IsSupportVideoFile(QString& fileName)
{
    QString endfix = fileName.mid(fileName.lastIndexOf("."));
    QStringList list;

    QString extList;
    m_DefaultCfg.GetNodeAttribute("DefaultConfig/Player/SupportFile", "extlist",extList);

    QString userExtList;
    m_UserCfg.GetNodeAttribute("UserConfig/Format", "extlist", userExtList);

    if (extList.length() < 2)
    {
        extList = ".mp4;.ts;.flv;.mkv;.mp3;.wav;";
    }
    list = extList.split(";");

    int i = 0;
    for (i = 0; i < list.size(); i++)
    {
        if ((list[i].length() > 2) && (list[i].startsWith('.')))
        {
            if (endfix.toLower() == list[i])
            {
                return true;
            }
        }
    }

    //判断一下用户定义的后缀名
    list = userExtList.split(";");
    for (i = 0; i < list.size(); i++)
    {
        if ((list[i].length() > 2) && (list[i].startsWith('.')))
        {
            if (endfix.toLower() == list[i])
            {
                return true;
            }
        }
    }
    return false;
}
