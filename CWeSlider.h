#ifndef CWEQSLIDER_H
#define CWEQSLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QCoreApplication>


enum
{
    //背景
    AnQSlider_TYPE_BK = 0,
    //已经播放的进度
    AnQSlider_TYPE_USED,
    //后台进度
    AnQSlider_TYPE_WORKPROCESS,

    AnQSlider_TYPE_MAX
};

class CWeSlider : public QSlider
{
    Q_OBJECT
public:
    CWeSlider(QWidget *parent = 0) : QSlider(parent)
    {
        m_Color[AnQSlider_TYPE_BK] = QColor(50, 50, 50, 255);
        m_Color[AnQSlider_TYPE_USED] = QColor(0x00, 0xB1, 0xEA, 255);
        m_Color[AnQSlider_TYPE_WORKPROCESS] = QColor(100, 100, 100, 255);

        m_ValueWorkProcess = 0;

        m_EnableWorkerProcess = false;
        m_EnableTips = false;
        m_isPressed = false;
        m_DragPos = 0;

        this->setMouseTracking(true);
    }

    void EnableWorkerProcess(bool bEnable);

    void EnableTips(bool bEnable);

    int GetLineSize(void);

protected:
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);

    virtual void paintEvent(QPaintEvent*);

signals:
    void anqSliderClicked(qint64 pos);

private:
    int m_ValueWorkProcess;

    bool m_EnableWorkerProcess;
    bool m_EnableTips;

    bool m_isPressed;
    int  m_DragPos;

    QColor m_Color[AnQSlider_TYPE_MAX];
};


#endif // ANQSLIDER_H
