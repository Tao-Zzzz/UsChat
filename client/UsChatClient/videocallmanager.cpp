#include "VideoCallManager.h"
#include "TcpMgr.h"
#include "usermgr.h"
#include <QJsonDocument>
#include <QTimer>
#include "VideoCallWidget.h"
#include "WebRtcJsBridge.h"

namespace
{
// 仅用于当前进程内防重入；你的 Manager 是单例，这么用没问题
int g_programmaticQtHangupDepth = 0;
bool g_acceptPending = false;


void SafeEmitQtHangup(WebRtcJsBridge* bridge)
{
    ++g_programmaticQtHangupDepth;
    emit bridge->qtHangup();
    --g_programmaticQtHangupDepth;
}

bool IsProgrammaticQtHangupInProgress()
{
    return g_programmaticQtHangupDepth > 0;
}
}

static CallMediaType FromMediaTypeValue(int value)
{
    // 安全判断，防止非法值
    switch (value)
    {
    case 0:
        return CallMediaType::Audio;
    case 1:
        return CallMediaType::Video;
    default:
        // 默认给语音，更安全
        return CallMediaType::Video;
    }
}

WebRtcJsBridge* VideoCallManager::CurrentBridge() const
{
    return (_mediaType == CallMediaType::Video)
    ? WebRtcJsBridge::GetVideoInstance()
    : WebRtcJsBridge::GetVoiceInstance();
}

VideoCallManager* VideoCallManager::GetInstance()
{
    static VideoCallManager instance;
    return &instance;
}

VideoCallManager::VideoCallManager(QObject *parent)
    : QObject(parent)
{
    auto videoBridge = WebRtcJsBridge::GetVideoInstance();
    auto voiceBridge = WebRtcJsBridge::GetVoiceInstance();

    auto bindBridge = [this](WebRtcJsBridge* bridge) {
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
            if (IsProgrammaticQtHangupInProgress()) {
                return;
            }

            if (_state == CallState::Connecting || _state == CallState::InCall) {
                const int selfUid = _selfUid;
                const int peerUid = _peerUid;
                const QString callId = _callId;

                EmitCallLocalHangupByMediaType();

                if (!callId.isEmpty()) {
                    QJsonObject jsonObj;
                    jsonObj["uid"] = selfUid;
                    jsonObj["other_id"] = peerUid;
                    jsonObj["call_id"] = callId;

                    QJsonDocument doc(jsonObj);
                    emit TcpMgr::GetInstance()->sig_send_data(
                                                   ID_VIDEO_HANGUP_REQ,
                                                   doc.toJson(QJsonDocument::Compact)
                                                   );
                }

                Reset();
            }
        });

        connect(bridge, &WebRtcJsBridge::sigCallError, this, [this](const QString& err) {
            EmitCallErrorByMediaType(err);
        });
    };

    bindBridge(videoBridge);
    bindBridge(voiceBridge);
}

void VideoCallManager::StartCall(int selfUid, std::shared_ptr<UserInfo> other_user_info, CallMediaType mediaType)
{
    if (_state != CallState::Idle) {
        EmitCallErrorByMediaType(QStringLiteral("当前已有通话进行中"));
        return;
    }

    g_acceptPending = false;

    _selfUid = selfUid;
    _peerUid = other_user_info->_uid;

    _callId.clear();
    _state = CallState::Calling;
    _browserRtcStarted = false;
    _rtcStarted = false;

    _peer_user_info = other_user_info;
    _mediaType = mediaType;

    QJsonObject jsonObj;
    jsonObj["uid"] = selfUid;
    jsonObj["other_id"] = other_user_info->_uid;
    jsonObj["call_type"] = static_cast<int>(_mediaType);

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_INVITE_REQ, doc.toJson(QJsonDocument::Compact));
    EmitShowCallingUiByMediaType();
    EmitCallCanCancelByMediaType(false);
}

void VideoCallManager::AcceptCall()
{
    if (_state != CallState::Ringing || _callId.isEmpty()) {
        return;
    }

    if (g_acceptPending) {
        return;
    }
    g_acceptPending = true;

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_ACCEPT_REQ, doc.toJson(QJsonDocument::Compact));

    // 不要在这里提前切状态，等服务端 accept rsp 成功后再切
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

    SafeEmitQtHangup(CurrentBridge());
    Reset();
}

void VideoCallManager::CancelCall()
{
    if (_state != CallState::Calling) {
        return;
    }

    if (_callId.isEmpty()) {
        EmitCallCancelledByMediaType();
        SafeEmitQtHangup(CurrentBridge());
        Reset();
        return;
    }

    QJsonObject jsonObj;
    jsonObj["uid"] = _selfUid;
    jsonObj["other_id"] = _peerUid;
    jsonObj["call_id"] = _callId;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_CANCEL_REQ, doc.toJson(QJsonDocument::Compact));

    EmitCallCancelledByMediaType();
    SafeEmitQtHangup(CurrentBridge());
    Reset();
}

void VideoCallManager::Hangup()
{
    if ((_state != CallState::Connecting && _state != CallState::InCall) || _callId.isEmpty()) {
        SafeEmitQtHangup(CurrentBridge());
        Reset();
        return;
    }

    const int selfUid = _selfUid;
    const int peerUid = _peerUid;
    const QString callId = _callId;

    SafeEmitQtHangup(CurrentBridge());

    QJsonObject jsonObj;
    jsonObj["uid"] = selfUid;
    jsonObj["other_id"] = peerUid;
    jsonObj["call_id"] = callId;

    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ID_VIDEO_HANGUP_REQ, doc.toJson(QJsonDocument::Compact));

    EmitCallLocalHangupByMediaType();
    Reset();
}

void VideoCallManager::HandleVideoInviteRsp(const QJsonObject& obj)
{
    int err = obj.value("error").toInt();
    if (err != 0) {
        EmitCallErrorByMediaType(QStringLiteral("发起%1失败，错误码: %2")
                                .arg(_mediaType == CallMediaType::Video
                                         ? QStringLiteral("视频通话邀请")
                                         : QStringLiteral("语音通话邀请"))
                                .arg(err));
        SafeEmitQtHangup(CurrentBridge());
        Reset();
        return;
    }

    _callId = obj.value("call_id").toString();
    EmitCallCanCancelByMediaType(true);
}

void VideoCallManager::HandleVideoAcceptRsp(const QJsonObject& obj)
{
    g_acceptPending = false;

    int err = obj.value("error").toInt();
    if (err != 0) {
        EmitCallErrorByMediaType(QStringLiteral("发起%1失败，错误码: %2")
                                .arg(_mediaType == CallMediaType::Video
                                         ? QStringLiteral("视频通话邀请")
                                         : QStringLiteral("语音通话邀请"))
                                .arg(err));
        SafeEmitQtHangup(CurrentBridge());
        Reset();
        return;
    }

    const QString rspCallId = obj.value("call_id").toString();
    if (!rspCallId.isEmpty() && !_callId.isEmpty() && rspCallId != _callId) {
        return;
    }

    _state = CallState::Connecting;
    EmitCallAcceptedByMediaType();

    // 这是发给被叫 B 的
    StartBrowserRtcAsCalleeIfNeeded();
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
    g_acceptPending = false;

    _callId = obj.value("call_id").toString();
    _peerUid = obj.value("from_uid").toInt();
    _selfUid = UserMgr::GetInstance()->GetUid();
    _state = CallState::Ringing;
    _browserRtcStarted = false;
    _rtcStarted = false;


    _peer_user_info = UserMgr::GetInstance()->GetFriendById(_peerUid);


    if (obj.contains("call_type")) {
        _mediaType = FromMediaTypeValue(obj["call_type"].toInt());
    } else {
        _mediaType = CallMediaType::Video;
    }

    EmitShowIncomingUiByMediaType(obj);
}

void VideoCallManager::HandleNotifyVideoAccept(const QJsonObject& obj)
{
    const QString notifyCallId = obj.value("call_id").toString();
    if (!notifyCallId.isEmpty() && !_callId.isEmpty() && notifyCallId != _callId) {
        return;
    }

    if (_state != CallState::Calling && _state != CallState::Connecting) {
        return;
    }

    _state = CallState::Connecting;
    EmitCallAcceptedByMediaType();

    // 这是发给主叫 A 的
    StartBrowserRtcAsCallerIfNeeded();
}

void VideoCallManager::HandleNotifyVideoReject(const QJsonObject& obj)
{
    const QString notifyCallId = obj.value("call_id").toString();
    if (!notifyCallId.isEmpty() && !_callId.isEmpty() && notifyCallId != _callId) {
        return;
    }

    EmitCallRejectedByMediaType();
    SafeEmitQtHangup(CurrentBridge());
    Reset();
}

void VideoCallManager::HandleNotifyVideoCancel(const QJsonObject& obj)
{
    const QString notifyCallId = obj.value("call_id").toString();
    if (!notifyCallId.isEmpty() && !_callId.isEmpty() && notifyCallId != _callId) {
        return;
    }

    EmitCallCancelledByMediaType();
    SafeEmitQtHangup(CurrentBridge());
    Reset();
}

void VideoCallManager::HandleNotifyVideoHangup(const QJsonObject& obj)
{
    const QString notifyCallId = obj.value("call_id").toString();
    if (!notifyCallId.isEmpty() && !_callId.isEmpty() && notifyCallId != _callId) {
        return;
    }

    SafeEmitQtHangup(CurrentBridge());
    EmitCallHangupByMediaType();
    Reset();
}

void VideoCallManager::SendOffer(const QString& sdp)
{
    if (_callId.isEmpty() || _peerUid <= 0) {
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
    if (_callId.isEmpty() || _peerUid <= 0) {
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
    if (_callId.isEmpty() || _peerUid <= 0) {
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
    const QString msgCallId = obj.value("call_id").toString();
    if (_callId.isEmpty() || msgCallId != _callId) {
        return;
    }

    if (_state != CallState::Connecting && _state != CallState::InCall) {
        return;
    }

    const QString sdp = obj.value("sdp").toString();
    if (sdp.isEmpty()) {
        return;
    }

    emit CurrentBridge()->qtSetRemoteOffer(sdp);
}

void VideoCallManager::HandleNotifyAnswer(const QJsonObject& obj)
{
    const QString msgCallId = obj.value("call_id").toString();
    if (_callId.isEmpty() || msgCallId != _callId) {
        return;
    }

    if (_state != CallState::Connecting && _state != CallState::InCall) {
        return;
    }

    const QString sdp = obj.value("sdp").toString();
    if (sdp.isEmpty()) {
        return;
    }

    emit CurrentBridge()->qtSetRemoteAnswer(sdp);
}

void VideoCallManager::HandleNotifyIceCandidate(const QJsonObject& obj)
{
    const QString msgCallId = obj.value("call_id").toString();
    if (_callId.isEmpty() || msgCallId != _callId) {
        return;
    }

    if (_state != CallState::Connecting && _state != CallState::InCall) {
        return;
    }

    emit CurrentBridge()->qtAddRemoteIce(
                             obj.value("candidate").toString(),
                             obj.value("sdpMid").toString(),
                             obj.value("sdpMLineIndex").toInt()
                             );
}

void VideoCallManager::MarkCallConnected()
{
    if (_state != CallState::Connecting && _state != CallState::InCall) {
        return;
    }

    _state = CallState::InCall;
    EmitCallConnectedByMediaType();
}

void VideoCallManager::Reset()
{
    g_acceptPending = false;
    _browserRtcStarted = false;
    _rtcStarted = false;
    _state = CallState::Idle;
    _selfUid = 0;
    _peerUid = 0;
    _callId.clear();
    _mediaType = CallMediaType::Video;
}

void VideoCallManager::StartBrowserRtcAsCallerIfNeeded()
{
    if (_browserRtcStarted) {
        return;
    }

    _browserRtcStarted = true;
    emit CurrentBridge()->qtSetMediaType(static_cast<int>(_mediaType));
    emit CurrentBridge()->qtStartCaller();
}

void VideoCallManager::StartBrowserRtcAsCalleeIfNeeded()
{
    if (_browserRtcStarted) {
        return;
    }

    _browserRtcStarted = true;
    emit CurrentBridge()->qtSetMediaType(static_cast<int>(_mediaType));
    emit CurrentBridge()->qtStartCallee();
}

void VideoCallManager::EmitShowCallingUiByMediaType()
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_show_calling_ui();
    } else {
        emit sig_voice_show_calling_ui();
    }
}

void VideoCallManager::EmitShowIncomingUiByMediaType(const QJsonObject& obj)
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_show_incoming_ui(obj);
    } else {
        emit sig_voice_show_incoming_ui(obj);
    }
}

void VideoCallManager::EmitCallAcceptedByMediaType()
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_accepted();
    } else {
        emit sig_voice_call_accepted();
    }
}

void VideoCallManager::EmitCallRejectedByMediaType()
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_rejected();
    } else {
        emit sig_voice_call_rejected();
    }
}

void VideoCallManager::EmitCallHangupByMediaType()
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_hangup();
    } else {
        emit sig_voice_call_hangup();
    }
}

void VideoCallManager::EmitCallConnectedByMediaType()
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_connected();
    } else {
        emit sig_voice_call_connected();
    }
}

void VideoCallManager::EmitRecvOfferByMediaType(const QString& sdp)
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_recv_offer(sdp);
    } else {
        emit sig_voice_recv_offer(sdp);
    }
}

void VideoCallManager::EmitRecvAnswerByMediaType(const QString& sdp)
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_recv_answer(sdp);
    } else {
        emit sig_voice_recv_answer(sdp);
    }
}

void VideoCallManager::EmitRecvIceCandidateByMediaType(const QString& candidate, const QString& sdpMid, int sdpMLineIndex)
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_recv_ice_candidate(candidate, sdpMid, sdpMLineIndex);
    } else {
        emit sig_voice_recv_ice_candidate(candidate, sdpMid, sdpMLineIndex);
    }
}

void VideoCallManager::EmitCallErrorByMediaType(const QString& text)
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_error(text);
    } else {
        emit sig_voice_call_error(text);
    }
}

void VideoCallManager::EmitCallCancelledByMediaType()
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_cancelled();
    } else {
        emit sig_voice_call_cancelled();
    }
}

void VideoCallManager::EmitCallLocalHangupByMediaType()
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_local_hangup();
    } else {
        emit sig_voice_call_local_hangup();
    }
}

void VideoCallManager::EmitCallCanCancelByMediaType(bool enable)
{
    if (_mediaType == CallMediaType::Video) {
        emit sig_video_call_can_cancel(enable);
    } else {
        emit sig_voice_call_can_cancel(enable);
    }
}
