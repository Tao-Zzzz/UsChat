#ifndef WEBRTCMANAGER_H
#define WEBRTCMANAGER_H

#include <QWidget>

class WebRtcManager : public QObject
{
    Q_OBJECT
public:
    static WebRtcManager* GetInstance();

    void Init();
    void StartLocalCapture();
    void CreatePeerConnection();

    void CreateOffer();
    void CreateAnswer();

    void SetRemoteOffer(const QString& sdp);
    void SetRemoteAnswer(const QString& sdp);
    void AddRemoteIceCandidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);

signals:
    void sig_local_offer_created(const QString& sdp);
    void sig_local_answer_created(const QString& sdp);
    void sig_local_ice_candidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);
    void sig_connected();
};

#endif // WEBRTCMANAGER_H
