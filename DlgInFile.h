#ifndef DLGINFILE_H
#define DLGINFILE_H

#include <QDialog>

namespace Ui {
class DlgInFile;
}

class DlgInFile : public QDialog
{
    Q_OBJECT

public:
    explicit DlgInFile(QWidget *parent = 0);
    ~DlgInFile();

private:
    Ui::DlgInFile *ui;
};

#endif // DLGINFILE_H
