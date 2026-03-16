#include "VideoCallManager.h"
#include "TcpMgr.h"
#include "usermgr.h"
#include <QJsonDocument>
#include <QTimer>
#include "RtcEngine.h"
#include "VideoCallWidget.h"

VideoCallManager* VideoCallManager::GetInstance()
{
    static VideoCallManager instance;
    return &instance;
}

VideoCallManager::VideoCallManager(QObject *parent)
    : QObject(parent)
{
    auto rtc = RtcEngine::GetInstance();

    connect(this, &VideoCallManager::sig_recv_offer,
            rtc, &RtcEngine::OnRecvOffer);

    connect(this, &VideoCallManager::sig_recv_answer,
            rtc, &RtcEngine::OnRecvAnswer);

    connect(this, &VideoCallManager::sig_recv_ice_candidate,
            rtc, &RtcEngine::OnRecvIceCandidate);

    connect(rtc, &RtcEngine::sig_send_offer,
            this, &VideoCallManager::SendOffer);

    connect(rtc, &RtcEngine::sig_send_answer,
            this, &VideoCallManager::SendAnswer);

    connect(rtc, &RtcEngine::sig_connected, this, [this]() {
        MarkCallConnected();
    });

    connect(rtc, &RtcEngine::sig_disconnected, this, [this]() {
        emit sig_call_hangup();
        Reset();
    });

    connect(rtc, &RtcEngine::sig_error, this, [this](const QString& err) {
        emit sig_call_error(err);
        Reset();
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

    // 先关本地 RTC，但不要在这里触发 Reset
    RtcEngine::GetInstance()->Hangup();

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

    // 按当前状态判断自己身份，更稳
    if (oldState == CallState::Calling) {
        StartRtcAsCallerIfNeeded();
    } else {
        StartRtcAsCalleeIfNeeded();
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

    // 不死依赖 notify/rsp 名字，按当前状态判断
    if (oldState == CallState::Calling) {
        StartRtcAsCallerIfNeeded();
    } else {
        StartRtcAsCalleeIfNeeded();
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
    emit sig_recv_offer(obj.value("sdp").toString());
}

void VideoCallManager::HandleNotifyAnswer(const QJsonObject& obj)
{
    emit sig_recv_answer(obj.value("sdp").toString());
}

void VideoCallManager::HandleNotifyIceCandidate(const QJsonObject& obj)
{
    emit sig_recv_ice_candidate(
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

void VideoCallManager::StartRtcAsCallerIfNeeded()
{
    if (_rtcStarted) {
        return;
    }

    auto rtc = RtcEngine::GetInstance();
    auto view = VideoCallWidget::GetInstance();

    rtc->Init();
    rtc->SetLocalRenderWidget(view->GetLocalPreviewWidget());
    rtc->SetRemoteRenderWidget(view->GetRemoteVideoWidget());
    rtc->StartPreview();
    rtc->StartCallAsCaller();

    _rtcStarted = true;
}

void VideoCallManager::StartRtcAsCalleeIfNeeded()
{
    if (_rtcStarted) {
        return;
    }

    auto rtc = RtcEngine::GetInstance();
    auto view = VideoCallWidget::GetInstance();

    rtc->Init();
    rtc->SetLocalRenderWidget(view->GetLocalPreviewWidget());
    rtc->SetRemoteRenderWidget(view->GetRemoteVideoWidget());
    rtc->StartPreview();
    rtc->StartCallAsCallee();

    _rtcStarted = true;
}

void VideoCallManager::Reset()
{
    RtcEngine::GetInstance()->Reset();
    _rtcStarted = false;
    _state = CallState::Idle;
    _peerUid = 0;
    _callId.clear();
}
