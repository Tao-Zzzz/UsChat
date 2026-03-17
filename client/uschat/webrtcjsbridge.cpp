#include "WebRtcJsBridge.h"

WebRtcJsBridge* WebRtcJsBridge::GetInstance()
{
    static WebRtcJsBridge instance;
    return &instance;
}

WebRtcJsBridge::WebRtcJsBridge(QObject* parent)
    : QObject(parent)
{
}

void WebRtcJsBridge::jsReady()
{
    // 先留空。后续如果你要做“页面 ready 后再收 offer/answer/ice”，可以从这里扩展。
}

void WebRtcJsBridge::jsSendOffer(const QString& sdp)
{
    emit sigRemoteOffer(sdp);
}

void WebRtcJsBridge::jsSendAnswer(const QString& sdp)
{
    emit sigRemoteAnswer(sdp);
}

void WebRtcJsBridge::jsSendIce(const QString& candidate, const QString& sdpMid, int sdpMLineIndex)
{
    emit sigRemoteIce(candidate, sdpMid, sdpMLineIndex);
}

void WebRtcJsBridge::jsNotifyConnected()
{
    emit sigCallConnected();
}

void WebRtcJsBridge::jsNotifyHangup()
{
    emit sigHangup();
}

void WebRtcJsBridge::jsNotifyError(const QString& msg)
{
    emit sigCallError(msg);
}
