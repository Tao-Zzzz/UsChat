#include "RtcEngine.h"

#include <QMetaObject>
#include <QDebug>

// 下面这些 include 名称，可能需要你按 libwebrtc 仓库实际头文件微调
#include "libwebrtc/rtc_peerconnection_factory.h"
#include "libwebrtc/rtc_peerconnection.h"
#include "libwebrtc/rtc_peerconnection_observer.h"
#include "libwebrtc/rtc_configuration.h"
#include "libwebrtc/scoped_refptr.h"
#include "libwebrtc/portable_string.h"
#include "libwebrtc/portable_vector.h"

using namespace libwebrtc;

class RtcEngine::PeerObserver : public RTCPeerConnectionObserver
{
public:
    explicit PeerObserver(RtcEngine* engine)
        : _engine(engine)
    {
    }

    void OnIceCandidate(const portable::string& candidate,
                        const portable::string& sdpMid,
                        int sdpMLineIndex) override
    {
        if (!_engine) {
            return;
        }

        QString qCandidate = QString::fromStdString(candidate);
        QString qSdpMid = QString::fromStdString(sdpMid);

        QMetaObject::invokeMethod(_engine, [this, qCandidate, qSdpMid, sdpMLineIndex]() {
            if (_engine) {
                _engine->OnLocalIceCandidate(qCandidate, qSdpMid, sdpMLineIndex);
            }
        }, Qt::QueuedConnection);
    }

    void OnPeerConnectionState(int state) override
    {
        if (!_engine) {
            return;
        }

        QMetaObject::invokeMethod(_engine, [this, state]() {
            if (_engine) {
                _engine->OnPeerStateChanged(state);
            }
        }, Qt::QueuedConnection);
    }

    void OnTrack() override
    {
        if (!_engine) {
            return;
        }

        QMetaObject::invokeMethod(_engine, [this]() {
            if (_engine) {
                _engine->OnRemoteTrack();
            }
        }, Qt::QueuedConnection);
    }

private:
    RtcEngine* _engine = nullptr;
};

RtcEngine* RtcEngine::GetInstance()
{
    static RtcEngine instance;
    return &instance;
}

RtcEngine::RtcEngine(QObject* parent)
    : QObject(parent)
{
}

RtcEngine::~RtcEngine()
{
    Reset();
}

bool RtcEngine::Init()
{
    if (_inited) {
        return true;
    }

    if (!CreateFactoryIfNeeded()) {
        emit sig_error(QStringLiteral("RTC Factory 初始化失败"));
        return false;
    }

    _inited = true;
    return true;
}

bool RtcEngine::CreateFactoryIfNeeded()
{
    if (_factory) {
        return true;
    }

    // 按你给的规范：Factory 负责 Initialize / Create
    _factory = RTCPeerConnectionFactory::Create();
    if (!_factory) {
        return false;
    }

    if (!_factory->Initialize()) {
        _factory = nullptr;
        return false;
    }

    return true;
}

bool RtcEngine::CreatePeerConnectionIfNeeded()
{
    if (_pc) {
        return true;
    }

    if (!_factory && !CreateFactoryIfNeeded()) {
        return false;
    }

    RTCConfiguration config;

    // 下面先只给一个最小配置
    // 你后面可以把 stun/turn 从配置文件读出来
    config.sdp_semantics = RTCConfiguration::SdpSemantics::UnifiedPlan;
    config.enable_dtls_srtp = true;

    RTCIceServer iceServer;
    iceServer.urls.push_back("stun:stun.l.google.com:19302");
    config.ice_servers.push_back(iceServer);

    _observer = new PeerObserver(this);

    // 这里的方法名可能需要你按仓库实际接口微调
    _pc = _factory->CreatePeerConnection(config);
    if (!_pc) {
        delete _observer;
        _observer = nullptr;
        return false;
    }

    _pc->RegisterRTCPeerConnectionObserver(_observer);

    // ===== 媒体轨添加 =====
    // 这里按你给的规范，Factory 有 CreateAudioSource / CreateVideoSource
    // 真正的 track 创建函数名可能依仓库实现略有不同。
    // 先按常见封装写，如果编译时报 track 函数名不一致，把报错贴我，我给你对齐。
    auto audioSource = _factory->CreateAudioSource();
    auto videoSource = _factory->CreateVideoSource();

    if (audioSource) {
        auto audioTrack = _factory->CreateAudioTrack("audio_track", audioSource);
        if (audioTrack) {
            _pc->AddTrack(audioTrack, { "stream_0" });
        }
    }

    if (videoSource) {
        auto videoTrack = _factory->CreateVideoTrack("video_track", videoSource);
        if (videoTrack) {
            _pc->AddTrack(videoTrack, { "stream_0" });
        }
    }

    return true;
}

void RtcEngine::DestroyPeerConnection()
{
    if (_pc) {
        _pc->Close();
        _pc = nullptr;
    }

    if (_observer) {
        delete _observer;
        _observer = nullptr;
    }
}

void RtcEngine::Reset()
{
    DestroyPeerConnection();
    _isCaller = false;
}

void RtcEngine::Hangup()
{
    DestroyPeerConnection();
}

void RtcEngine::SetLocalRenderWidget(QWidget* widget)
{
    _localRenderWidget = widget;
}

void RtcEngine::SetRemoteRenderWidget(QWidget* widget)
{
    _remoteRenderWidget = widget;
}

void RtcEngine::StartPreview()
{
    if (!Init()) {
        return;
    }

    // 这里先不强写渲染逻辑
    // 因为你给的规范里没有 renderer / sink API
    // 等你把 wrapper 的视频渲染类名发我，再把本地视频绑定到 _localRenderWidget
}

void RtcEngine::StopPreview()
{
    // 同上，后续接渲染时补
}

void RtcEngine::StartCallAsCaller()
{
    if (!Init()) {
        return;
    }

    if (!CreatePeerConnectionIfNeeded()) {
        emit sig_error(QStringLiteral("创建 PeerConnection 失败"));
        return;
    }

    _isCaller = true;
    CreateOffer();
}

void RtcEngine::StartCallAsCallee()
{
    if (!Init()) {
        return;
    }

    if (!CreatePeerConnectionIfNeeded()) {
        emit sig_error(QStringLiteral("创建 PeerConnection 失败"));
        return;
    }

    _isCaller = false;
}

void RtcEngine::CreateOffer()
{
    if (!_pc) {
        emit sig_error(QStringLiteral("PeerConnection 未创建"));
        return;
    }

    RTCOfferAnswerOptions options;

    _pc->CreateOffer(
        [this](const portable::string sdp, const portable::string type) {
            QString qSdp = QString::fromStdString(sdp);
            QString qType = QString::fromStdString(type);

            QMetaObject::invokeMethod(this, [this, qSdp, qType]() {
                SetLocalDescription(qSdp, qType);
                emit sig_send_offer(qSdp);
            }, Qt::QueuedConnection);
        },
        [this](const char* error) {
            QString err = error ? QString::fromUtf8(error) : QStringLiteral("CreateOffer failed");
            QMetaObject::invokeMethod(this, [this, err]() {
                emit sig_error(err);
            }, Qt::QueuedConnection);
        },
        options
        );
}

void RtcEngine::CreateAnswer()
{
    if (!_pc) {
        emit sig_error(QStringLiteral("PeerConnection 未创建"));
        return;
    }

    RTCOfferAnswerOptions options;

    _pc->CreateAnswer(
        [this](const portable::string sdp, const portable::string type) {
            QString qSdp = QString::fromStdString(sdp);
            QString qType = QString::fromStdString(type);

            QMetaObject::invokeMethod(this, [this, qSdp, qType]() {
                SetLocalDescription(qSdp, qType);
                emit sig_send_answer(qSdp);
            }, Qt::QueuedConnection);
        },
        [this](const char* error) {
            QString err = error ? QString::fromUtf8(error) : QStringLiteral("CreateAnswer failed");
            QMetaObject::invokeMethod(this, [this, err]() {
                emit sig_error(err);
            }, Qt::QueuedConnection);
        },
        options
        );
}

void RtcEngine::SetLocalDescription(const QString& sdp, const QString& type)
{
    if (!_pc) {
        return;
    }

    portable::string psdp = sdp.toStdString();
    portable::string ptype = type.toStdString();

    _pc->SetLocalDescription(
        psdp,
        ptype,
        []() {
        },
        [this](const char* error) {
            QString err = error ? QString::fromUtf8(error) : QStringLiteral("SetLocalDescription failed");
            QMetaObject::invokeMethod(this, [this, err]() {
                emit sig_error(err);
            }, Qt::QueuedConnection);
        }
        );
}

void RtcEngine::SetRemoteDescription(const QString& sdp, const QString& type)
{
    if (!_pc) {
        emit sig_error(QStringLiteral("PeerConnection 未创建"));
        return;
    }

    portable::string psdp = sdp.toStdString();
    portable::string ptype = type.toStdString();

    _pc->SetRemoteDescription(
        psdp,
        ptype,
        [this, type]() {
            QMetaObject::invokeMethod(this, [this, type]() {
                if (type == QStringLiteral("offer")) {
                    CreateAnswer();
                }
            }, Qt::QueuedConnection);
        },
        [this](const char* error) {
            QString err = error ? QString::fromUtf8(error) : QStringLiteral("SetRemoteDescription failed");
            QMetaObject::invokeMethod(this, [this, err]() {
                emit sig_error(err);
            }, Qt::QueuedConnection);
        }
        );
}

void RtcEngine::OnRecvOffer(const QString& sdp)
{
    if (!Init()) {
        return;
    }

    if (!CreatePeerConnectionIfNeeded()) {
        emit sig_error(QStringLiteral("创建 PeerConnection 失败"));
        return;
    }

    _isCaller = false;
    SetRemoteDescription(sdp, QStringLiteral("offer"));
}

void RtcEngine::OnRecvAnswer(const QString& sdp)
{
    if (!_pc) {
        emit sig_error(QStringLiteral("收到 answer 时 PeerConnection 未创建"));
        return;
    }

    SetRemoteDescription(sdp, QStringLiteral("answer"));
}

void RtcEngine::OnRecvIceCandidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex)
{
    if (!_pc) {
        return;
    }

    _pc->AddCandidate(candidate.toStdString(),
                      sdpMid.toStdString(),
                      sdpMLineIndex);
}

void RtcEngine::OnLocalIceCandidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex)
{
    emit sig_send_ice_candidate(candidate, sdpMid, sdpMLineIndex);
}

void RtcEngine::OnPeerStateChanged(int state)
{
    // 这里按常见状态约定处理
    // 如果你仓库里有明确的 enum，后面可以替换成 enum 名
    switch (state) {
    case 3: // connected
        emit sig_connected();
        break;
    case 4: // disconnected
    case 5: // failed
    case 6: // closed
        emit sig_disconnected();
        break;
    default:
        break;
    }
}

void RtcEngine::OnRemoteTrack()
{
    qDebug() << "[RtcEngine] remote track received";
    // 这里先只留事件点
    // 后面接 renderer 时，把远端 track 绑到 _remoteRenderWidget
}
