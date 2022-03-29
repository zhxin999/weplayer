#ifndef CWELISTWIDGET_H
#define CWELISTWIDGET_H

#include <QListWidget>

class CWeListWidget: public QListWidget
{
    Q_OBJECT
public:
    CWeListWidget(QWidget *parent = 0);

    virtual QStringList mimeTypes() const;
    virtual bool dropMimeData(int index, const QMimeData *data, Qt::DropAction action);

signals:
    void notifyDropNewFile(int pos, QString szFileName);

protected:
    void dropEvent(QDropEvent*);
    void dragEnterEvent(QDragEnterEvent*);

};

#endif // CWELISTWIDGET_H
