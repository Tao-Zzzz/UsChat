#include "WebRtcJsBridge.h"

WebRtcJsBridge* WebRtcJsBridge::GetVideoInstance()
{
    static WebRtcJsBridge instance;
    return &instance;
}

WebRtcJsBridge* WebRtcJsBridge::GetVoiceInstance()
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
