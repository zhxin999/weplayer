#ifndef DLGOPTION_H
#define DLGOPTION_H

#include <QDialog>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionTab>

namespace Ui {
class DlgOption;
}
class OPTION_MOVE
{
public:
    bool m_move;
    QPoint m_startPoint;
    QPoint m_windowPoint;
    qint64 m_pressTime;
};

class CustomTabStyle : public QProxyStyle
{
public:
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
        const QSize &size, const QWidget *widget) const
    {
        QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == QStyle::CT_TabBarTab) {
            s.transpose();
#ifdef AV_OS_WIN32
            s.rwidth() = 100; // ����ÿ��tabBar��item�Ĵ�С
#else
            s.rwidth() = 100;
#endif
            s.rheight() = 30;
        }
        return s;
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        if (element == CE_TabBarTabLabel) {
            if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
                QRect allRect = tab->rect;

                if (tab->state & QStyle::State_Selected) {
                    painter->save();
                    //painter->setPen(0x00B1EA);
                    //painter->setBrush(QBrush(0x00B1EA));
                    //painter->setPen(0x474A51);
                    //painter->setBrush(QBrush(0x1A1A1B));
                    //ainter->drawRect(allRect.adjusted(19, 5, -18, -5));
#ifdef AV_OS_WIN32
                    painter->setPen(0x474A51);
                    painter->setBrush(QBrush(0x474A51));
                    painter->drawRect(allRect.adjusted(0, 0, 0, 0));
#else
                    painter->setPen(0x474A51);
                    painter->setBrush(QBrush(0x474A51));
                    painter->drawRect(allRect.adjusted(0, 0, 0, 0));
#endif

                    painter->restore();
                }
                else {
#ifdef AV_OS_WIN32
                    painter->setPen(0x1A1A1B);
                    painter->setBrush(QBrush(0x1A1A1B));
                    painter->drawRect(allRect.adjusted(0, 0, 0, 0));

#else
                    painter->setPen(0x1A1A1B);
                    painter->setBrush(QBrush(0x1A1A1B));
                    painter->drawRect(allRect.adjusted(0, 0, 0, 0));
#endif
                }

                QTextOption option;
                option.setAlignment(Qt::AlignCenter);
                if (tab->state & QStyle::State_Selected) {
                    painter->setPen(0xF0F0F0);
                }
                else {
                    painter->setPen(0xF0F0F0);
                }

                painter->drawText(allRect, tab->text, option);
                return;
            }
        }

        if (element == CE_TabBarTab) {
            QProxyStyle::drawControl(element, option, painter, widget);
        }
    }
};


class DlgOption : public QDialog
{
    Q_OBJECT

public:
    explicit DlgOption(QWidget *parent = 0);
    ~DlgOption();

    int SetHardAccelMode(int mode);
    int GetHardAccelMode();
    int SetFormatExt(QString format);
    int SetUserFormat(QString format);
    QString GetUserFormat();

private slots:
    void on_btnClose_clicked();
    void on_btnSaveTabPlayer_clicked();

    void on_lblSourceCode_linkActivated(const QString &link);

    void on_btnSaveFormat_clicked();

private:
    bool eventFilter(QObject *, QEvent *);

private:
    Ui::DlgOption *ui;
    OPTION_MOVE m_WindowMove;
    int m_retCode;
};

#endif // DLGOPTION_H
