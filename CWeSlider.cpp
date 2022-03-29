#include "CWeSlider.h"
#include <QColor>
#include <QPainter>
#include <QDebug>
#include "anIncludes.h"

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

void CWeSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    m_isPressed = false;
    //QSlider::mousePressEvent(ev);
    double pos = ev->pos().x() / (double)width();
    qint64 val = pos * (maximum() - minimum()) + minimum();
    //double pos = ev->pos().x() / (double)width();
    //setValue(pos * (maximum() - minimum()) + minimum());

    emit anqSliderClicked(val);
}
void CWeSlider::mousePressEvent(QMouseEvent *ev)
{
    m_isPressed = true;
}
void CWeSlider::mouseMoveEvent(QMouseEvent *ev)
{
    double pos = ev->pos().x() / (double)width();
    qint64 val = pos * (maximum() - minimum()) + minimum();
    char szMsg[256];
    int secods = val / 1000;

    if (m_isPressed)
    {
        m_DragPos = val;
        this->repaint();

        snprintf(szMsg, sizeof(szMsg), "跳转%02d:%02d:%02d", secods/3600, (secods%3600)/60, secods%60);
        this->setToolTip(szMsg);
    }
    else
    {
        if (m_EnableTips)
        {
            snprintf(szMsg, sizeof(szMsg), "跳转%02d:%02d:%02d", secods/3600, (secods%3600)/60, secods%60);
            this->setToolTip(szMsg);
        }
        else
        {
            this->setToolTip(QString(""));
        }
    }


}

void CWeSlider::paintEvent(QPaintEvent* evt)
{
    QPainter paint(this);
    int Width, Height;
    int64_t CurPos, WorkerPos;
    int Max, Min, Value;
    int MiddleHeight = GetScaledSize(4);

    Max = this->maximum();
    Min = this->minimum();

    if (m_isPressed)
    {
        Value = m_DragPos;
    }
    else
    {
        Value = this->value();
    }


    Width = this->width();
    Height = this->height();

    if ((Max - Min) == 0)
    {
        paint.setPen(m_Color[AnQSlider_TYPE_BK]);
        paint.setBrush(QBrush(m_Color[AnQSlider_TYPE_BK]));

        paint.drawRect(0, 0, Width, Height);

        CurPos = 0;
    }
    else
    {
        CurPos = ((int64_t)Value * (int64_t)Width)/(int64_t)(Max - Min);
        if (CurPos < Height/4)
        {
            CurPos = Height/4;
        }
        else if (CurPos > Width - Height/4)
        {
            CurPos = Width - Height/4;
        }

        paint.setPen(QColor(0, 0, 0, 0));
        paint.setBrush(QBrush(QColor(0, 0, 0, 0)));
        paint.drawRect(0, 0, Width, Height);

        paint.setPen(m_Color[AnQSlider_TYPE_BK]);
        paint.setBrush(QBrush(m_Color[AnQSlider_TYPE_BK]));
        paint.drawRect(0, Height/2 - MiddleHeight/2, Width, MiddleHeight);

        paint.setPen(m_Color[AnQSlider_TYPE_USED]);
        paint.setBrush(QBrush(m_Color[AnQSlider_TYPE_USED]));
        paint.drawRect(0, Height/2 - MiddleHeight/2, (int)CurPos, MiddleHeight);

        if (m_EnableWorkerProcess)
        {
            WorkerPos = ((int64_t)m_ValueWorkProcess * (int64_t)Width)/(int64_t)(Max - Min);

            if (WorkerPos > 0)
            {
                paint.setPen(m_Color[AnQSlider_TYPE_WORKPROCESS]);
                paint.setBrush(QBrush(m_Color[AnQSlider_TYPE_WORKPROCESS], Qt::SolidPattern));

                paint.drawRect((int)CurPos, Height/4, (int)(WorkerPos - CurPos), Height/2);
            }
            else
            {
                WorkerPos = CurPos;
            }
        }
        else
        {
            WorkerPos = CurPos;
        }

        //绘制图标
        paint.setPen(QPen(Qt::white,4,Qt::SolidLine));
        paint.setBrush(QBrush(Qt::white, Qt::SolidPattern));

        paint.setRenderHint(QPainter::Antialiasing, true);

        paint.drawEllipse(QPoint((int)CurPos, Height/2), Height/4, Height/4);
    }

}
void CWeSlider::EnableWorkerProcess(bool bEnable)
{
    m_EnableWorkerProcess = bEnable;
}

void CWeSlider::EnableTips(bool bEnable)
{
     m_EnableTips = bEnable;
}
int CWeSlider::GetLineSize(void)
{
    return GetScaledSize(4);
}
