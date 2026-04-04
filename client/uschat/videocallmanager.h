#ifndef VIDEOCALLMANAGER_H
#define VIDEOCALLMANAGER_H
#include <QObject>
#include <QJsonObject>
#include <QString>
#include "userdata.h"

enum class CallState
{
    Idle,
    Calling,
    Ringing,
    Connecting,
    InCall,
    Ended
};

class VideoCallManager : public QObject
{
    Q_OBJECT
public:
    static VideoCallManager* GetInstance();

    void StartCall(int selfUid, std::shared_ptr<UserInfo> user_info, CallMediaType mediaType);
    void AcceptCall();
    void RejectCall();
    void CancelCall();
    void Hangup();

    void SendOffer(const QString& sdp);
    void SendAnswer(const QString& sdp);
    void SendIceCandidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);

    void HandleVideoInviteRsp(const QJsonObject& obj);
    void HandleVideoAcceptRsp(const QJsonObject& obj);
    void HandleVideoRejectRsp(const QJsonObject& obj);
    void HandleVideoCancelRsp(const QJsonObject& obj);
    void HandleVideoHangupRsp(const QJsonObject& obj);

    void HandleNotifyVideoInvite(const QJsonObject& obj);
    void HandleNotifyVideoAccept(const QJsonObject& obj);
    void HandleNotifyVideoReject(const QJsonObject& obj);
    void HandleNotifyVideoCancel(const QJsonObject& obj);
    void HandleNotifyVideoHangup(const QJsonObject& obj);

    void HandleNotifyOffer(const QJsonObject& obj);
    void HandleNotifyAnswer(const QJsonObject& obj);
    void HandleNotifyIceCandidate(const QJsonObject& obj);

    CallState GetState() const { return _state; }
    QString GetCallId() const { return _callId; }
    int GetPeerUid() const { return _peerUid; }
    QString GetPeerName() const {

        if(_peer_user_info == nullptr){
            return nullptr;
        }

        return _peer_user_info->_name;
    }
    int GetSelfUid() const { return _selfUid; }

    // WebRTC 真正接通后可主动调用这个
    void MarkCallConnected();

    void StartBrowserRtcAsCallerIfNeeded();
    void StartBrowserRtcAsCalleeIfNeeded();

    CallMediaType GetMediaType() const { return _mediaType; }
    bool IsAudioCall() const { return _mediaType == CallMediaType::Audio; }
    bool IsVideoCall() const { return _mediaType == CallMediaType::Video; }

signals:
    void sig_show_calling_ui();
    void sig_show_incoming_ui(const QJsonObject& obj);
    void sig_call_accepted();
    void sig_call_rejected();
    void sig_call_hangup();
    void sig_call_connected();

    void sig_recv_offer(const QString& sdp);
    void sig_recv_answer(const QString& sdp);
    void sig_recv_ice_candidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);

    // 给 UI 用的额外状态
    void sig_call_error(const QString& text);
    void sig_call_cancelled();
    void sig_call_local_hangup();
    void sig_call_can_cancel(bool enable);

private:
    explicit VideoCallManager(QObject *parent = nullptr);
    void Reset();
    void StartRtcAsCallerIfNeeded();
    void StartRtcAsCalleeIfNeeded();
private:
    CallState _state = CallState::Idle;
    int _selfUid = 0;
    int _peerUid = 0;
    std::shared_ptr<UserInfo> _peer_user_info;

    QString _callId;
    bool _rtcStarted = false;
    bool _browserRtcStarted = false;

    CallMediaType _mediaType = CallMediaType::Video;
};

#endif // VIDEOCALLMANAGER_H
