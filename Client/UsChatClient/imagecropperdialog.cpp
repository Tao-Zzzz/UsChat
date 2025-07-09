#include "imagecropperdialog.h"
#include "ui_imagecropperdialog.h"

ImageCropperDialog::ImageCropperDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImageCropperDialog)
{
    ui->setupUi(this);
}

ImageCropperDialog::~ImageCropperDialog()
{
    delete ui;
}
