#include "DlgInFile.h"
#include "ui_dlginfile.h"

DlgInFile::DlgInFile(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgInFile)
{
    ui->setupUi(this);
}

DlgInFile::~DlgInFile()
{
    delete ui;
}
