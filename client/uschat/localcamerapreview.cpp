
#include "localcamerapreview.h"
#include <QCamera>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QVBoxLayout>
#include <QLabel>

LocalCameraPreview::LocalCameraPreview(QWidget *parent)
    : QWidget(parent)
{
    InitUi();
}

LocalCameraPreview::~LocalCameraPreview()
{
    StopPreview();
}

void LocalCameraPreview::InitUi()
{
    setMinimumSize(120, 160);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _viewfinder = new QCameraViewfinder(this);
    _viewfinder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(_viewfinder);

    _overlayText = new QLabel(QStringLiteral("本地预览"), this);
    _overlayText->setAlignment(Qt::AlignCenter);
    _overlayText->setStyleSheet(
        "background-color: rgba(0,0,0,120);"
        "color: white;"
        "font-size: 14px;"
        "border-radius: 8px;"
        "padding: 6px;"
        );
    _overlayText->raise();
}

bool LocalCameraPreview::StartPreview()
{
    if (_started) {
        return true;
    }

    auto cameras = QCameraInfo::availableCameras();
    if (cameras.isEmpty()) {
        ShowText(QStringLiteral("未检测到摄像头"));
        return false;
    }

    _camera = new QCamera(cameras.first(), this);
    _camera->setViewfinder(_viewfinder);
    _camera->start();

    _started = true;
    HideText();
    return true;
}

void LocalCameraPreview::StopPreview()
{
    if (_camera) {
        _camera->stop();
        _camera->deleteLater();
        _camera = nullptr;
    }

    _started = false;
    ShowText(QStringLiteral("本地预览"));
}

bool LocalCameraPreview::IsStarted() const
{
    return _started;
}

void LocalCameraPreview::ShowText(const QString& text)
{
    if (!_overlayText) {
        return;
    }

    _overlayText->setText(text);
    _overlayText->setGeometry(10, 10, width() - 20, 36);
    _overlayText->show();
}

void LocalCameraPreview::HideText()
{
    if (_overlayText) {
        _overlayText->hide();
    }
}

void LocalCameraPreview::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (_overlayText) {
        _overlayText->setGeometry(10, 10, width() - 20, 36);
    }
}
