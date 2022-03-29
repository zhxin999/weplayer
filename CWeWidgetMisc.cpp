#include "CWeWidgetMisc.h"


#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

CWarningText::CWarningText(QWidget *parent)
    : QLabel(parent)
{
    _timer=new QTimer(this);
    connect(_timer,SIGNAL(timeout()),this,SLOT(timer_Ticks()));
    _timer->start(200);
    m_timeout = 0;

    this->setStyleSheet("background-color: rgba(67, 67, 67, 200);color:#FFFFFFFF;padding-left:4px;padding-right:10px;");
    this->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    this->setMargin(10);
}

CWarningText::~CWarningText()
{
    if (_timer)
    {
        delete _timer;
        _timer = NULL;
    }
}
void CWarningText::timer_Ticks()
{
    if (m_timeout > 0)
    {
        m_timeout--;
    }
    else
    {
        if (this->isVisible())
        {
            this->hide();
        }
    }


}
void CWarningText::setTextTimeout(const QString& txt, int TimeoutMs)
{
    QString strText =QString("<html><head/><body><p align=\"left\" style=\"margin-top: 30px;\"><span style=\" font-size:14pt; font-weight:600; color:#ffffff;\">");
    strText += txt;
    strText += QString("</span></p></body></html>");

    this->setText(strText);
    this->adjustSize();
    this->show();
    m_timeout = TimeoutMs/200;
}

void CWarningText::ShowHtmlTextTimeout(const QString& txt, int TimeoutMs)
{
    QString strText =QString("<html><head/><body>");
    strText += txt;
    strText += QString("</body></html>");

    this->setText(strText);

    //QFontMetrics fontMetrics(this->font());

    //int nFontWidth = fontMetrics.width(txt);

    //this->setMaximumWidth(nFontWidth + 20);
    this->adjustSize();

    this->show();
    m_timeout = TimeoutMs/200;
}
