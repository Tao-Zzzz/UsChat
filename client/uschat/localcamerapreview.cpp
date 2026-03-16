#include "localcamerapreview.h"

#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QDebug>

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

    _videoWidget = new QVideoWidget(this);
    _videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(_videoWidget);

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

    const auto cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        ShowText(QStringLiteral("未检测到摄像头"));
        return false;
    }

    qDebug() << "启动摄像头";

    _camera = new QCamera(cameras.first(), this);
    _captureSession = new QMediaCaptureSession(this);

    _captureSession->setCamera(_camera);
    _captureSession->setVideoOutput(_videoWidget);

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

    if (_captureSession) {
        _captureSession->deleteLater();
        _captureSession = nullptr;
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
