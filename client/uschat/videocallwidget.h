#ifndef VIDEOCALLWIDGET_H
#define VIDEOCALLWIDGET_H
#include <QWidget>
#include <QJsonObject>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class LocalCameraPreview;

class VideoCallWidget : public QWidget
{
    Q_OBJECT
public:
    static VideoCallWidget* GetInstance();

    explicit VideoCallWidget(QWidget *parent = nullptr);

    void ShowCalling();
    void ShowIncoming(const QJsonObject& obj);
    void ShowConnecting();
    void ShowInCall();
    void ShowEnd(const QString& text = QStringLiteral("通话已结束"));

    void SetPeerName(const QString& name);
    void SetStatusText(const QString& text);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void slot_accept();
    void slot_reject();
    void slot_cancel();
    void slot_hangup();
    void slot_update_call_time();

private:
    void InitUi();
    QWidget* CreateCallingPage();
    QWidget* CreateIncomingPage();
    QWidget* CreateInCallPage();
    void SwitchToPage(int index);

    void StartLocalPreview();
    void StopLocalPreview();

    void StartCallTimer();
    void StopCallTimer();

private:
    enum PageIndex
    {
        PAGE_CALLING = 0,
        PAGE_INCOMING = 1,
        PAGE_INCALL = 2
    };

private:
    QStackedWidget* _stack = nullptr;

    QLabel* _labCallingPeerName = nullptr;
    QLabel* _labCallingStatus = nullptr;
    QPushButton* _btnCancel = nullptr;

    QLabel* _labIncomingPeerName = nullptr;
    QLabel* _labIncomingStatus = nullptr;
    QPushButton* _btnAccept = nullptr;
    QPushButton* _btnReject = nullptr;

    QLabel* _labInCallPeerName = nullptr;
    QLabel* _labInCallStatus = nullptr;
    QLabel* _labCallTime = nullptr;
    QWidget* _remoteVideoView = nullptr;
    LocalCameraPreview* _localPreview = nullptr;
    QPushButton* _btnHangup = nullptr;

    QTimer* _callTimer = nullptr;
    int _callSeconds = 0;
};

#endif // VIDEOCALLWIDGET_H
