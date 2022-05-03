#include "dlgoption.h"
#include "ui_dlgoption.h"
#include <QMouseEvent>
#include <QDesktopServices>

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

DlgOption::DlgOption(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgOption)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);

    ui->tabOptions->setTabPosition(QTabWidget::West);
    ui->tabOptions->tabBar()->setStyle(new CustomTabStyle);
    ui->tabOptions->setStyleSheet("QTabWidget::pane { border: 0; }");

    ui->tabOptions->setCurrentIndex(0);

    ui->lblTitle->setAttribute(Qt::WA_TranslucentBackground, true);
    ui->lblTitle->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    this->installEventFilter(this);

    m_retCode = 0;
}

DlgOption::~DlgOption()
{
    delete ui;
}

bool DlgOption::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = (QMouseEvent *)event;
        QPoint p = mouseEvent->pos();
        if (mouseEvent->button() == Qt::LeftButton)
        {
            m_WindowMove.m_move = true;
            m_WindowMove.m_startPoint = mouseEvent->globalPos();
            m_WindowMove.m_windowPoint = this->frameGeometry().topLeft();
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = (QMouseEvent *)event;

        if (m_WindowMove.m_move)
        {
            QPoint relativePos = mouseEvent->globalPos() - m_WindowMove.m_startPoint;
            this->move(m_WindowMove.m_windowPoint + relativePos);
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        m_WindowMove.m_move = false;
    }

    return QWidget::eventFilter(watched, event);
}
void DlgOption::on_btnClose_clicked()
{
    this->done(0);
}


void DlgOption::on_btnSaveTabPlayer_clicked()
{
    m_retCode = 1;
    this->done(m_retCode);
}

int DlgOption::SetHardAccelMode(int mode)
{
    if (mode == 0)
    {
        ui->chkHardAccel->setChecked(false);
    }
    else
    {
        ui->chkHardAccel->setChecked(true);
    }

    return 0;
}

int DlgOption::GetHardAccelMode()
{
    if (ui->chkHardAccel->isChecked() == true)
    {
        return 1;
    }

    return 0;
}

void DlgOption::on_lblSourceCode_linkActivated(const QString &link)
{
    QDesktopServices::openUrl(QUrl(link));
}

int DlgOption::SetFormatExt(QString format)
{
    ui->editFormatExt->setText(format);
    return 0;
}

int DlgOption::SetUserFormat(QString format)
{
    ui->leUserFormat->setText(format);
    return 0;
}
QString DlgOption::GetUserFormat()
{
    return ui->leUserFormat->text();
}

void DlgOption::on_btnSaveFormat_clicked()
{
    m_retCode = 1;
    this->done(m_retCode);
}
