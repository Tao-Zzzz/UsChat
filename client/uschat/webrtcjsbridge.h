#ifndef WEBRTCJSBRIDGE_H
#define WEBRTCJSBRIDGE_H

#include <QObject>
#include <QString>

class WebRtcJsBridge : public QObject
{
    Q_OBJECT
public:
    static WebRtcJsBridge* GetInstance();
    explicit WebRtcJsBridge(QObject* parent = nullptr);

signals:
    void sigStartCaller();
    void sigStartCallee();
    void sigRemoteOffer(const QString& sdp);
    void sigRemoteAnswer(const QString& sdp);
    void sigRemoteIce(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);
    void sigHangup();

    void sigCallConnected();
    void sigCallError(const QString& msg);

public slots:
    // JS -> Qt
    void jsReady();
    void jsSendOffer(const QString& sdp);
    void jsSendAnswer(const QString& sdp);
    void jsSendIce(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);
    void jsNotifyConnected();
    void jsNotifyHangup();
    void jsNotifyError(const QString& msg);

signals:
    // Qt -> JS
    void qtStartCaller();
    void qtStartCallee();
    void qtSetRemoteOffer(const QString& sdp);
    void qtSetRemoteAnswer(const QString& sdp);
    void qtAddRemoteIce(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);
    void qtHangup();
    void qtStartPreview();
};

#endif // WEBRTCJSBRIDGE_H
