#include "VideoCallManager.h"
#include "TcpMgr.h"
#include "usermgr.h"
#include <QJsonDocument>
#include <QTimer>
#include "browserrtcbridge.h"
#include "VideoCallWidget.h"
#include "WebRtcJsBridge.h"

VideoCallManager* VideoCallManager::GetInstance()
{
    static VideoCallManager instance;
    return &instance;
}

VideoCallManager::VideoCallManager(QObject *parent)
    : QObject(parent)
{
    auto bridge = WebRtcJsBridge::GetInstance();

    connect(bridge, &WebRtcJsBridge::sigRemoteOffer,
            this, &VideoCallManager::SendOffer);

    connect(bridge, &WebRtcJsBridge::sigRemoteAnswer,
            this, &VideoCallManager::SendAnswer);

    connect(bridge, &WebRtcJsBridge::sigRemoteIce,
            this, &VideoCallManager::SendIceCandidate);

    connect(bridge, &WebRtcJsBridge::sigCallConnected, this, [this]() {
        MarkCallConnected();
    });

    connect(bridge, &WebRtcJsBridge::sigHangup, this, [this]() {
        if (_state == CallState::Connecting || _state == CallState::InCall) {
            Hangup();
        }
    });

    connect(bridge, &WebRtcJsBridge::sigCallError, this, [this](const QString& err) {
        emit sig_call_error(err);
    });
}

void VideoCallManager::StartCall(int selfUid, int otherUid)
{
    if (_state != CallState::Idle) {
        emit sig_call_error(QStringLiteral("当前已有通话进行中"));
        return;
    }

    _selfUid = selfUid;
    _peerUid = otherUid;
    _callId.clear();
    _state = CallState::Calling;

    QJsonObject jsonObj;
    jsonObj["uid"] = selfUid;
    jsonObj["other_id"] = otherUid;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_INVITE_REQ, doc.toJson(QJsonDocument::Compact));
    emit sig_show_calling_ui();
    emit sig_call_can_cancel(false);
}

void VideoCallManager::AcceptCall()
{
    if (_state != CallState::Ringing || _callId.isEmpty()) {
        return;
    }

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_ACCEPT_REQ, doc.toJson(QJsonDocument::Compact));
    _state = CallState::Connecting;
}

void VideoCallManager::RejectCall()
{
    if (_state != CallState::Ringing || _callId.isEmpty()) {
        Reset();
        return;
    }

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_REJECT_REQ, doc.toJson(QJsonDocument::Compact));
    Reset();
}

void VideoCallManager::CancelCall()
{
    // 允许在 Calling 状态取消
    if (_state != CallState::Calling) {
        return;
    }

    // 如果 call_id 还没拿到，直接本地重置
    if (_callId.isEmpty()) {
        emit sig_call_cancelled();
        Reset();
        return;
    }

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_CANCEL_REQ, doc.toJson(QJsonDocument::Compact));
    emit sig_call_cancelled();
    Reset();
}

void VideoCallManager::Hangup()
{
    if ((_state != CallState::Connecting && _state != CallState::InCall) || _callId.isEmpty()) {
        Reset();
        return;
    }

    const int selfUid = _selfUid;
    const int peerUid = _peerUid;
    const QString callId = _callId;

    emit WebRtcJsBridge::GetInstance()->qtHangup();

    QJsonObject jsonObj;
    jsonObj["uid"] = selfUid;
    jsonObj["other_id"] = peerUid;
    jsonObj["call_id"] = callId;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_HANGUP_REQ, doc.toJson(QJsonDocument::Compact));

    emit sig_call_local_hangup();
    Reset();
}

void VideoCallManager::HandleVideoInviteRsp(const QJsonObject& obj)
{
    int err = obj.value("error").toInt();
    if (err != 0) {
        emit sig_call_error(QStringLiteral("发起视频邀请失败，错误码: %1").arg(err));
        Reset();
        return;
    }

    _callId = obj.value("call_id").toString();
    emit sig_call_can_cancel(true);
}

void VideoCallManager::HandleVideoAcceptRsp(const QJsonObject& obj)
{
    int err = obj.value("error").toInt();
    if (err != 0) {
        emit sig_call_error(QStringLiteral("接听失败，错误码: %1").arg(err));
        Reset();
        return;
    }

    CallState oldState = _state;
    _state = CallState::Connecting;
    emit sig_call_accepted();

    if (oldState == CallState::Calling) {
        StartBrowserRtcAsCallerIfNeeded();
    } else {
        StartBrowserRtcAsCalleeIfNeeded();
    }
}

void VideoCallManager::HandleVideoRejectRsp(const QJsonObject& obj)
{
    Q_UNUSED(obj);
}

void VideoCallManager::HandleVideoCancelRsp(const QJsonObject& obj)
{
    Q_UNUSED(obj);
}

void VideoCallManager::HandleVideoHangupRsp(const QJsonObject& obj)
{
    Q_UNUSED(obj);
}

void VideoCallManager::HandleNotifyVideoInvite(const QJsonObject& obj)
{
    _callId = obj.value("call_id").toString();
    _peerUid = obj.value("from_uid").toInt();
    _selfUid = UserMgr::GetInstance()->GetUid();
    _state = CallState::Ringing;
    emit sig_show_incoming_ui(obj);
}

void VideoCallManager::HandleNotifyVideoAccept(const QJsonObject& obj)
{
    Q_UNUSED(obj);

    CallState oldState = _state;
    _state = CallState::Connecting;
    emit sig_call_accepted();

    if (oldState == CallState::Calling) {
        StartBrowserRtcAsCallerIfNeeded();
    } else {
        StartBrowserRtcAsCalleeIfNeeded();
    }
}

void VideoCallManager::HandleNotifyVideoReject(const QJsonObject& obj)
{
    Q_UNUSED(obj);
    emit sig_call_rejected();
    Reset();
}

void VideoCallManager::HandleNotifyVideoCancel(const QJsonObject& obj)
{
    Q_UNUSED(obj);
    emit sig_call_cancelled();
    Reset();
}

void VideoCallManager::HandleNotifyVideoHangup(const QJsonObject& obj)
{
    Q_UNUSED(obj);
    emit WebRtcJsBridge::GetInstance()->qtHangup();
    emit sig_call_hangup();
    Reset();
}

void VideoCallManager::SendOffer(const QString& sdp)
{
    if (_callId.isEmpty()) {
        return;
    }

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;
    jsonObj["sdp"] = sdp;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_WEBRTC_OFFER_REQ, doc.toJson(QJsonDocument::Compact));
}

void VideoCallManager::SendAnswer(const QString& sdp)
{
    if (_callId.isEmpty()) {
        return;
    }

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;
    jsonObj["sdp"] = sdp;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_WEBRTC_ANSWER_REQ, doc.toJson(QJsonDocument::Compact));
}

void VideoCallManager::SendIceCandidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex)
{
    if (_callId.isEmpty()) {
        return;
    }

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;
    jsonObj["candidate"] = candidate;
    jsonObj["sdpMid"] = sdpMid;
    jsonObj["sdpMLineIndex"] = sdpMLineIndex;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_WEBRTC_ICE_CANDIDATE_REQ, doc.toJson(QJsonDocument::Compact));
}

void VideoCallManager::HandleNotifyOffer(const QJsonObject& obj)
{
    const QString sdp = obj.value("sdp").toString();
    emit WebRtcJsBridge::GetInstance()->qtSetRemoteOffer(sdp);
}

void VideoCallManager::HandleNotifyAnswer(const QJsonObject& obj)
{
    const QString sdp = obj.value("sdp").toString();
    emit WebRtcJsBridge::GetInstance()->qtSetRemoteAnswer(sdp);
}

void VideoCallManager::HandleNotifyIceCandidate(const QJsonObject& obj)
{
    emit WebRtcJsBridge::GetInstance()->qtAddRemoteIce(
                                           obj.value("candidate").toString(),
                                           obj.value("sdpMid").toString(),
                                           obj.value("sdpMLineIndex").toInt()
                                           );
}


void VideoCallManager::MarkCallConnected()
{
    _state = CallState::InCall;
    emit sig_call_connected();
}


void VideoCallManager::Reset()
{
    emit WebRtcJsBridge::GetInstance()->qtHangup();

    _browserRtcStarted = false;
    _rtcStarted = false;
    _state = CallState::Idle;
    _peerUid = 0;
    _callId.clear();
}

void VideoCallManager::StartBrowserRtcAsCallerIfNeeded()
{
    if (_browserRtcStarted) {
        return;
    }

    _browserRtcStarted = true;
    emit WebRtcJsBridge::GetInstance()->qtStartCaller();
}

void VideoCallManager::StartBrowserRtcAsCalleeIfNeeded()
{
    if (_browserRtcStarted) {
        return;
    }

    _browserRtcStarted = true;
    emit WebRtcJsBridge::GetInstance()->qtStartCallee();
}

