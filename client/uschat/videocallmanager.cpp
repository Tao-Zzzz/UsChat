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

void SafeEmitQtHangup()
{
    ++g_programmaticQtHangupDepth;
    emit WebRtcJsBridge::GetInstance()->qtHangup();
    --g_programmaticQtHangupDepth;
}

bool IsProgrammaticQtHangupInProgress()
{
    return g_programmaticQtHangupDepth > 0;
}
}

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

    // 关键修复：
    // 网页侧的 hangup 不能再无脑回调到 Hangup()，否则会和 qtHangup 形成递归/重入
    connect(bridge, &WebRtcJsBridge::sigHangup, this, [this]() {
        // 如果这是 Qt 主动下发的 qtHangup 触发出来的回调，直接忽略
        if (IsProgrammaticQtHangupInProgress()) {
            return;
        }

        // 只有在真的还处于通话中时，才把它当成“本地 RTC 意外关闭”
        if (_state == CallState::Connecting || _state == CallState::InCall) {
            const int selfUid = _selfUid;
            const int peerUid = _peerUid;
            const QString callId = _callId;

            emit sig_call_local_hangup();

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
        emit sig_call_error(err);
    });
}

void VideoCallManager::StartCall(int selfUid, std::shared_ptr<UserInfo> other_user_info)
{
    if (_state != CallState::Idle) {
        emit sig_call_error(QStringLiteral("当前已有通话进行中"));
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

    QJsonObject jsonObj;
    jsonObj["uid"] = selfUid;
    jsonObj["other_id"] = other_user_info->_uid;

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

    SafeEmitQtHangup();
    Reset();
}

void VideoCallManager::CancelCall()
{
    if (_state != CallState::Calling) {
        return;
    }

    if (_callId.isEmpty()) {
        emit sig_call_cancelled();
        SafeEmitQtHangup();
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
    SafeEmitQtHangup();
    Reset();
}

void VideoCallManager::Hangup()
{
    if ((_state != CallState::Connecting && _state != CallState::InCall) || _callId.isEmpty()) {
        SafeEmitQtHangup();
        Reset();
        return;
    }

    const int selfUid = _selfUid;
    const int peerUid = _peerUid;
    const QString callId = _callId;

    SafeEmitQtHangup();

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
        SafeEmitQtHangup();
        Reset();
        return;
    }

    _callId = obj.value("call_id").toString();
    emit sig_call_can_cancel(true);
}

void VideoCallManager::HandleVideoAcceptRsp(const QJsonObject& obj)
{
    g_acceptPending = false;

    int err = obj.value("error").toInt();
    if (err != 0) {
        emit sig_call_error(QStringLiteral("接听失败，错误码: %1").arg(err));
        SafeEmitQtHangup();
        Reset();
        return;
    }

    const QString rspCallId = obj.value("call_id").toString();
    if (!rspCallId.isEmpty() && !_callId.isEmpty() && rspCallId != _callId) {
        return;
    }

    _state = CallState::Connecting;
    emit sig_call_accepted();

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



    emit sig_show_incoming_ui(obj);
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
    emit sig_call_accepted();

    // 这是发给主叫 A 的
    StartBrowserRtcAsCallerIfNeeded();
}

void VideoCallManager::HandleNotifyVideoReject(const QJsonObject& obj)
{
    const QString notifyCallId = obj.value("call_id").toString();
    if (!notifyCallId.isEmpty() && !_callId.isEmpty() && notifyCallId != _callId) {
        return;
    }

    emit sig_call_rejected();
    SafeEmitQtHangup();
    Reset();
}

void VideoCallManager::HandleNotifyVideoCancel(const QJsonObject& obj)
{
    const QString notifyCallId = obj.value("call_id").toString();
    if (!notifyCallId.isEmpty() && !_callId.isEmpty() && notifyCallId != _callId) {
        return;
    }

    emit sig_call_cancelled();
    SafeEmitQtHangup();
    Reset();
}

void VideoCallManager::HandleNotifyVideoHangup(const QJsonObject& obj)
{
    const QString notifyCallId = obj.value("call_id").toString();
    if (!notifyCallId.isEmpty() && !_callId.isEmpty() && notifyCallId != _callId) {
        return;
    }

    SafeEmitQtHangup();
    emit sig_call_hangup();
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

    emit WebRtcJsBridge::GetInstance()->qtSetRemoteOffer(sdp);
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

    emit WebRtcJsBridge::GetInstance()->qtSetRemoteAnswer(sdp);
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

    emit WebRtcJsBridge::GetInstance()->qtAddRemoteIce(
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
    emit sig_call_connected();
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
