#ifndef VOICECALLWIDGET_H
#define VOICECALLWIDGET_H

#include <QWidget>
#include <QJsonObject>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QWebEngineView;
class QWebChannel;

class VoiceCallWidget : public QWidget
{
    Q_OBJECT
public:
    static VoiceCallWidget* GetInstance();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void slot_accept();
    void slot_reject();
    void slot_cancel();
    void slot_hangup();
    void slot_update_call_time();

private:
    explicit VoiceCallWidget(QWidget *parent = nullptr);

    void InitUi();
    QWidget* CreateCallingPage();
    QWidget* CreateIncomingPage();
    QWidget* CreateInCallPage();

    void ShowCalling();
    void ShowIncoming(const QJsonObject& obj);
    void ShowConnecting();
    void ShowInCall();
    void ShowEnd(const QString& text);

    void SetPeerName(const QString& name);
    void SetStatusText(const QString& text);

    void SwitchToPage(int index);
    void StartCallTimer();
    void StopCallTimer();
    void InitWebRtcPage();

private:
    enum
    {
        PAGE_CALLING = 0,
        PAGE_INCOMING,
        PAGE_INCALL
    };

private:
    QStackedWidget* _stack = nullptr;
    QTimer* _callTimer = nullptr;
    int _callSeconds = 0;

    QLabel* _labCallingPeerName = nullptr;
    QLabel* _labCallingStatus = nullptr;

    QLabel* _labIncomingPeerName = nullptr;
    QLabel* _labIncomingStatus = nullptr;

    QLabel* _labInCallPeerName = nullptr;
    QLabel* _labInCallStatus = nullptr;
    QLabel* _labCallTime = nullptr;

    QPushButton* _btnAccept = nullptr;
    QPushButton* _btnReject = nullptr;
    QPushButton* _btnCancel = nullptr;
    QPushButton* _btnHangup = nullptr;



    QWebEngineView* _webView = nullptr;
    QWebChannel* _webChannel = nullptr;
};

#endif // VOICECALLWIDGET_H
