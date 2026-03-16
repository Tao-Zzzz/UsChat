#ifndef RTCENGINE_H
#define RTCENGINE_H

#include <QObject>
#include <QString>

class QWidget;

namespace libwebrtc {
class RTCPeerConnectionFactory;
class RTCPeerConnection;
class RTCPeerConnectionObserver;
template <typename T>
class scoped_refptr;
}

class RtcEngine : public QObject
{
    Q_OBJECT
public:
    static RtcEngine* GetInstance();

    explicit RtcEngine(QObject* parent = nullptr);
    ~RtcEngine();

    bool Init();
    void Reset();
    void Hangup();

    void SetLocalRenderWidget(QWidget* widget);
    void SetRemoteRenderWidget(QWidget* widget);

    void StartPreview();
    void StopPreview();

    void StartCallAsCaller();
    void StartCallAsCallee();

    void OnRecvOffer(const QString& sdp);
    void OnRecvAnswer(const QString& sdp);
    void OnRecvIceCandidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);

signals:
    void sig_send_offer(const QString& sdp);
    void sig_send_answer(const QString& sdp);
    void sig_send_ice_candidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);

    void sig_connected();
    void sig_disconnected();
    void sig_error(const QString& err);

private:
    class PeerObserver;

private:
    bool CreateFactoryIfNeeded();
    bool CreatePeerConnectionIfNeeded();
    void DestroyPeerConnection();

    void CreateOffer();
    void CreateAnswer();

    void SetLocalDescription(const QString& sdp, const QString& type);
    void SetRemoteDescription(const QString& sdp, const QString& type);

    void OnLocalIceCandidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);
    void OnPeerStateChanged(int state);
    void OnRemoteTrack();

private:
    QWidget* _localRenderWidget = nullptr;
    QWidget* _remoteRenderWidget = nullptr;

    bool _inited = false;
    bool _isCaller = false;

    libwebrtc::scoped_refptr<libwebrtc::RTCPeerConnectionFactory> _factory;
    libwebrtc::scoped_refptr<libwebrtc::RTCPeerConnection> _pc;
    PeerObserver* _observer = nullptr;
};


#endif // RTCENGINE_H
