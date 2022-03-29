#include "CWeListWidget.h"
#include <QMimeData>
#include <QDebug>
#include <QDragEnterEvent>


#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif


CWeListWidget::CWeListWidget(QWidget *parent):QListWidget(parent)
{
    this->setAcceptDrops(true);
}
QStringList CWeListWidget::mimeTypes() const
{
    QStringList mimetypes = QListWidget::mimeTypes();

    mimetypes.append("url/file");
    return mimetypes;
}
bool CWeListWidget::dropMimeData(int index, const QMimeData *data, Qt::DropAction action)
{
    if (data->hasUrls())
    {
        return true;
    }
    else
    {
        return QListWidget::dropMimeData(index, data, action);
    }
}
void CWeListWidget::dropEvent(QDropEvent* evt)
{
    QListWidget::dropEvent(evt);
}
void CWeListWidget::dragEnterEvent(QDragEnterEvent* evt)
{
    QListWidget::dragEnterEvent(evt);
}
