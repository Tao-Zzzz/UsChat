#ifndef LOCALCAMERAPREVIEW_H
#define LOCALCAMERAPREVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QCamera;
class QVideoWidget;
class QLabel;
class QMediaCaptureSession;
class QResizeEvent;
QT_END_NAMESPACE

class LocalCameraPreview : public QWidget
{
    Q_OBJECT
public:
    explicit LocalCameraPreview(QWidget *parent = nullptr);
    ~LocalCameraPreview();

    bool StartPreview();
    void StopPreview();
    bool IsStarted() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void InitUi();
    void ShowText(const QString& text);
    void HideText();

private:
    QCamera* _camera = nullptr;
    QMediaCaptureSession* _captureSession = nullptr;
    QVideoWidget* _videoWidget = nullptr;
    QLabel* _overlayText = nullptr;
    bool _started = false;
};

#endif // LOCALCAMERAPREVIEW_H
