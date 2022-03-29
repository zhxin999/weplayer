#ifndef QWEWIDGETMISC_H
#define QWEWIDGETMISC_H
#include <QObject>
#include <QCoreApplication>
#include <qmouseeventtransition.h>
#include <qmutex.h>
#include <qtimer.h>
#include <QFrame>
#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

class CWarningText : public QLabel
{
    Q_OBJECT

public:
    CWarningText(QWidget *parent = 0);
    ~CWarningText();
    void setTextTimeout(const QString& txt, int TimeoutMs);
    void ShowHtmlTextTimeout(const QString& txt, int TimeoutMs);

private slots:
    void timer_Ticks();
private:
    QTimer *_timer;
    int m_timeout;
};



#endif // QWEWIDGETMISC_H
