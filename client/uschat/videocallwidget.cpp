#include "VideoCallWidget.h"
#include "VideoCallManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QFrame>
#include <QCloseEvent>
#include <QTimer>
#include <QApplication>
#include <QScreen>
#include <QWebEngineView>
#include <QWebChannel>
#include <QUrl>
#include "WebRtcJsBridge.h"

VideoCallWidget* VideoCallWidget::GetInstance()
{
    static VideoCallWidget instance;
    return &instance;
}

VideoCallWidget::VideoCallWidget(QWidget *parent)
    : QWidget(parent)
{
    InitUi();

    auto mgr = VideoCallManager::GetInstance();

    connect(mgr, &VideoCallManager::sig_show_calling_ui,
            this, &VideoCallWidget::ShowCalling);

    connect(mgr, &VideoCallManager::sig_show_incoming_ui,
            this, &VideoCallWidget::ShowIncoming);

    connect(mgr, &VideoCallManager::sig_call_accepted, this, [this]() {
        if (_btnAccept) {
            _btnAccept->setEnabled(true);
        }
        ShowConnecting();
    });

    connect(mgr, &VideoCallManager::sig_call_rejected, this, [this]() {
        if (_btnAccept) {
            _btnAccept->setEnabled(true);
        }
        ShowEnd(QStringLiteral("对方已拒绝"));
    });

    connect(mgr, &VideoCallManager::sig_call_hangup, this, [this]() {
        if (_btnAccept) {
            _btnAccept->setEnabled(true);
        }
        ShowEnd(QStringLiteral("对方已挂断"));
    });

    connect(mgr, &VideoCallManager::sig_call_connected, this, [this]() {
        if (_btnAccept) {
            _btnAccept->setEnabled(true);
        }
        ShowInCall();
    });

    connect(mgr, &VideoCallManager::sig_call_error, this, [this](const QString& text) {
        if (_btnAccept) {
            _btnAccept->setEnabled(true);
        }
        ShowEnd(text);
    });

    connect(mgr, &VideoCallManager::sig_call_cancelled, this, [this]() {
        if (_btnAccept) {
            _btnAccept->setEnabled(true);
        }
        ShowEnd(QStringLiteral("通话已取消"));
    });

    connect(mgr, &VideoCallManager::sig_call_local_hangup, this, [this]() {
        if (_btnAccept) {
            _btnAccept->setEnabled(true);
        }
        ShowEnd(QStringLiteral("通话已结束"));
    });

    connect(mgr, &VideoCallManager::sig_call_can_cancel, this, [this](bool enable) {
        if (_btnCancel) {
            _btnCancel->setEnabled(enable);
        }
    });
}

void VideoCallWidget::InitUi()
{
    setWindowTitle(QStringLiteral("视频通话"));
    setMinimumSize(420, 720);
    resize(420, 720);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    _stack = new QStackedWidget(this);
    _stack->addWidget(CreateCallingPage());
    _stack->addWidget(CreateIncomingPage());
    _stack->addWidget(CreateInCallPage());

    _callTimer = new QTimer(this);
    connect(_callTimer, &QTimer::timeout, this, &VideoCallWidget::slot_update_call_time);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(_stack);

    SwitchToPage(PAGE_CALLING);

    auto screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect rect = screen->availableGeometry();
        move(rect.center() - QPoint(width() / 2, height() / 2));
    }

    InitWebRtcPage();
}

QWidget* VideoCallWidget::CreateCallingPage()
{
    QWidget* page = new QWidget(this);
    page->setStyleSheet("background-color: #1f1f1f; color: white;");

    auto layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 40, 24, 32);
    layout->setSpacing(20);

    QLabel* title = new QLabel(QStringLiteral("视频通话"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 22px; font-weight: bold;");

    _labCallingPeerName = new QLabel(QStringLiteral("对方"), page);
    _labCallingPeerName->setAlignment(Qt::AlignCenter);
    _labCallingPeerName->setStyleSheet("font-size: 28px; font-weight: bold;");

    _labCallingStatus = new QLabel(QStringLiteral("正在呼叫对方..."), page);
    _labCallingStatus->setAlignment(Qt::AlignCenter);
    _labCallingStatus->setStyleSheet("font-size: 16px; color: #cfcfcf;");

    QFrame* preview = new QFrame(page);
    preview->setMinimumHeight(360);
    preview->setStyleSheet("background-color: #333333; border-radius: 12px;");

    auto previewLayout = new QVBoxLayout(preview);
    QLabel* previewText = new QLabel(QStringLiteral("等待对方接听后开启摄像头预览"), preview);
    previewText->setAlignment(Qt::AlignCenter);
    previewText->setStyleSheet("font-size: 18px; color: #bbbbbb;");
    previewLayout->addStretch();
    previewLayout->addWidget(previewText);
    previewLayout->addStretch();

    _btnCancel = new QPushButton(QStringLiteral("取消"), page);
    _btnCancel->setFixedHeight(46);
    _btnCancel->setStyleSheet(
        "QPushButton { background-color: #d9534f; color: white; border: none; border-radius: 8px; font-size: 16px; }"
        "QPushButton:hover { background-color: #c9302c; }"
        "QPushButton:disabled { background-color: #777777; color: #cccccc; }"
        );

    connect(_btnCancel, &QPushButton::clicked, this, &VideoCallWidget::slot_cancel);

    layout->addWidget(title);
    layout->addSpacing(16);
    layout->addWidget(_labCallingPeerName);
    layout->addWidget(_labCallingStatus);
    layout->addSpacing(8);
    layout->addWidget(preview, 1);
    layout->addSpacing(16);
    layout->addWidget(_btnCancel);

    return page;
}

QWidget* VideoCallWidget::CreateIncomingPage()
{
    QWidget* page = new QWidget(this);
    page->setStyleSheet("background-color: #1f1f1f; color: white;");

    auto layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 40, 24, 32);
    layout->setSpacing(20);

    QLabel* title = new QLabel(QStringLiteral("收到视频邀请"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 22px; font-weight: bold;");

    _labIncomingPeerName = new QLabel(QStringLiteral("对方"), page);
    _labIncomingPeerName->setAlignment(Qt::AlignCenter);
    _labIncomingPeerName->setStyleSheet("font-size: 28px; font-weight: bold;");

    _labIncomingStatus = new QLabel(QStringLiteral("邀请你进行视频通话"), page);
    _labIncomingStatus->setAlignment(Qt::AlignCenter);
    _labIncomingStatus->setStyleSheet("font-size: 16px; color: #cfcfcf;");

    QFrame* avatarArea = new QFrame(page);
    avatarArea->setMinimumHeight(360);
    avatarArea->setStyleSheet("background-color: #333333; border-radius: 12px;");
    auto avatarLayout = new QVBoxLayout(avatarArea);
    QLabel* avatarText = new QLabel(QStringLiteral("来电显示区域"), avatarArea);
    avatarText->setAlignment(Qt::AlignCenter);
    avatarText->setStyleSheet("font-size: 20px; color: #bbbbbb;");
    avatarLayout->addStretch();
    avatarLayout->addWidget(avatarText);
    avatarLayout->addStretch();

    auto btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(16);

    _btnReject = new QPushButton(QStringLiteral("拒绝"), page);
    _btnReject->setFixedHeight(46);
    _btnReject->setStyleSheet(
        "QPushButton { background-color: #d9534f; color: white; border: none; border-radius: 8px; font-size: 16px; }"
        "QPushButton:hover { background-color: #c9302c; }"
        );

    _btnAccept = new QPushButton(QStringLiteral("接听"), page);
    _btnAccept->setFixedHeight(46);
    _btnAccept->setStyleSheet(
        "QPushButton { background-color: #5cb85c; color: white; border: none; border-radius: 8px; font-size: 16px; }"
        "QPushButton:hover { background-color: #449d44; }"
        "QPushButton:disabled { background-color: #7fbf7f; color: #eeeeee; }"
        );

    connect(_btnReject, &QPushButton::clicked, this, &VideoCallWidget::slot_reject);
    connect(_btnAccept, &QPushButton::clicked, this, &VideoCallWidget::slot_accept);

    btnLayout->addWidget(_btnReject);
    btnLayout->addWidget(_btnAccept);

    layout->addWidget(title);
    layout->addSpacing(16);
    layout->addWidget(_labIncomingPeerName);
    layout->addWidget(_labIncomingStatus);
    layout->addSpacing(8);
    layout->addWidget(avatarArea, 1);
    layout->addSpacing(16);
    layout->addLayout(btnLayout);

    return page;
}

QWidget* VideoCallWidget::CreateInCallPage()
{
    QWidget* page = new QWidget(this);
    page->setStyleSheet("background-color: #111111; color: white;");

    auto layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 16, 12, 18);
    layout->setSpacing(12);

    _labInCallPeerName = new QLabel(QStringLiteral("通话中"), page);
    _labInCallPeerName->setAlignment(Qt::AlignCenter);
    _labInCallPeerName->setStyleSheet("font-size: 22px; font-weight: bold;");

    _labInCallStatus = new QLabel(QStringLiteral("正在建立连接..."), page);
    _labInCallStatus->setAlignment(Qt::AlignCenter);
    _labInCallStatus->setStyleSheet("font-size: 14px; color: #cfcfcf;");

    _labCallTime = new QLabel(QStringLiteral("00:00"), page);
    _labCallTime->setAlignment(Qt::AlignCenter);
    _labCallTime->setStyleSheet("font-size: 16px; color: #9fd3ff;");

    _remoteVideoView = new QWidget(page);
    _remoteVideoView->setMinimumHeight(520);
    _remoteVideoView->setStyleSheet("background-color: #2a2a2a; border-radius: 12px;");

    auto remoteLayout = new QVBoxLayout(_remoteVideoView);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    remoteLayout->setSpacing(0);

    _webView = new QWebEngineView(_remoteVideoView);
    remoteLayout->addWidget(_webView);

    _btnHangup = new QPushButton(QStringLiteral("挂断"), page);
    _btnHangup->setFixedHeight(46);
    _btnHangup->setStyleSheet(
        "QPushButton { background-color: #d9534f; color: white; border: none; border-radius: 8px; font-size: 16px; }"
        "QPushButton:hover { background-color: #c9302c; }"
        );

    connect(_btnHangup, &QPushButton::clicked, this, &VideoCallWidget::slot_hangup);

    layout->addWidget(_labInCallPeerName);
    layout->addWidget(_labInCallStatus);
    layout->addWidget(_labCallTime);
    layout->addWidget(_remoteVideoView, 1);
    layout->addWidget(_btnHangup);

    return page;
}

void VideoCallWidget::ShowCalling()
{
    auto mgr = VideoCallManager::GetInstance();
    SetPeerName(QStringLiteral("用户 %1").arg(mgr->GetPeerUid()));
    _labCallingStatus->setText(QStringLiteral("正在呼叫对方..."));
    _btnCancel->setEnabled(false);
    StopCallTimer();
    SwitchToPage(PAGE_CALLING);
    show();
    raise();
    activateWindow();

    // emit WebRtcJsBridge::GetInstance()->qtStartPreview();
}

void VideoCallWidget::ShowIncoming(const QJsonObject& obj)
{
    QString name = obj.value("nick").toString();
    if (name.isEmpty()) {
        name = obj.value("name").toString();
    }
    if (name.isEmpty()) {
        name = QStringLiteral("用户 %1").arg(obj.value("from_uid").toInt());
    }

    if (_btnAccept) {
        _btnAccept->setEnabled(true);
    }

    _labIncomingPeerName->setText(name);
    _labIncomingStatus->setText(QStringLiteral("邀请你进行视频通话"));
    StopCallTimer();
    SwitchToPage(PAGE_INCOMING);
    show();
    raise();
    activateWindow();
}

void VideoCallWidget::ShowConnecting()
{
    auto mgr = VideoCallManager::GetInstance();
    SetPeerName(QStringLiteral("用户 %1").arg(mgr->GetPeerUid()));

    _labInCallStatus->setText(QStringLiteral("正在建立连接..."));
    _labCallTime->setText(QStringLiteral("00:00"));
    StopCallTimer();
    SwitchToPage(PAGE_INCALL);
    show();
    raise();
    activateWindow();
}

void VideoCallWidget::ShowInCall()
{
    auto mgr = VideoCallManager::GetInstance();
    SetPeerName(QStringLiteral("用户 %1").arg(mgr->GetPeerUid()));

    _labInCallStatus->setText(QStringLiteral("通话中"));
    StartCallTimer();
    SwitchToPage(PAGE_INCALL);
    show();
    raise();
    activateWindow();
}

void VideoCallWidget::ShowEnd(const QString& text)
{
    _labInCallStatus->setText(text);
    StopCallTimer();

    SwitchToPage(PAGE_INCALL);

    QTimer::singleShot(1200, this, [this]() {
        this->hide();
    });
}

void VideoCallWidget::SetPeerName(const QString& name)
{
    if (_labCallingPeerName) {
        _labCallingPeerName->setText(name);
    }
    if (_labIncomingPeerName) {
        _labIncomingPeerName->setText(name);
    }
    if (_labInCallPeerName) {
        _labInCallPeerName->setText(name);
    }
}

void VideoCallWidget::SetStatusText(const QString& text)
{
    if (_stack->currentIndex() == PAGE_CALLING && _labCallingStatus) {
        _labCallingStatus->setText(text);
    } else if (_stack->currentIndex() == PAGE_INCOMING && _labIncomingStatus) {
        _labIncomingStatus->setText(text);
    } else if (_stack->currentIndex() == PAGE_INCALL && _labInCallStatus) {
        _labInCallStatus->setText(text);
    }
}

void VideoCallWidget::closeEvent(QCloseEvent *event)
{
    auto mgr = VideoCallManager::GetInstance();
    auto state = mgr->GetState();

    if (state == CallState::Calling) {
        mgr->CancelCall();
        event->ignore();
        hide();
        return;
    }

    if (state == CallState::Ringing) {
        mgr->RejectCall();
        event->ignore();
        hide();
        return;
    }

    if (state == CallState::Connecting || state == CallState::InCall) {
        mgr->Hangup();
        event->ignore();
        hide();
        return;
    }

    QWidget::closeEvent(event);
}

void VideoCallWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    UpdateInCallLocalPreviewGeometry();
}

void VideoCallWidget::slot_accept()
{
    if (_btnAccept) {
        _btnAccept->setEnabled(false);
    }
    VideoCallManager::GetInstance()->AcceptCall();
}

void VideoCallWidget::slot_reject()
{
    VideoCallManager::GetInstance()->RejectCall();
    hide();
}

void VideoCallWidget::slot_cancel()
{
    VideoCallManager::GetInstance()->CancelCall();
    hide();
}

void VideoCallWidget::slot_hangup()
{
    VideoCallManager::GetInstance()->Hangup();
    hide();
}

void VideoCallWidget::slot_update_call_time()
{
    ++_callSeconds;
    int mm = _callSeconds / 60;
    int ss = _callSeconds % 60;
    _labCallTime->setText(QString("%1:%2")
                              .arg(mm, 2, 10, QLatin1Char('0'))
                              .arg(ss, 2, 10, QLatin1Char('0')));
}

void VideoCallWidget::SwitchToPage(int index)
{
    _stack->setCurrentIndex(index);
}

void VideoCallWidget::StartCallTimer()
{
    _callSeconds = 0;
    _labCallTime->setText(QStringLiteral("00:00"));
    _callTimer->start(1000);
}

void VideoCallWidget::StopCallTimer()
{
    if (_callTimer) {
        _callTimer->stop();
    }
    _callSeconds = 0;
    if (_labCallTime) {
        _labCallTime->setText(QStringLiteral("00:00"));
    }
}

void VideoCallWidget::UpdateInCallLocalPreviewGeometry()
{
}

QWidget* VideoCallWidget::GetRemoteVideoWidget() const
{
    return _remoteVideoView;
}

void VideoCallWidget::InitWebRtcPage()
{
    if (!_webView) {
        return;
    }

    _webChannel = new QWebChannel(_webView->page());
    _webChannel->registerObject(QStringLiteral("WebRtcJsBridge"), WebRtcJsBridge::GetInstance());
    _webView->page()->setWebChannel(_webChannel);

    connect(_webView->page(), &QWebEnginePage::featurePermissionRequested,
            this, [this](const QUrl &securityOrigin, QWebEnginePage::Feature feature) {
                if (feature == QWebEnginePage::MediaAudioCapture ||
                    feature == QWebEnginePage::MediaVideoCapture ||
                    feature == QWebEnginePage::MediaAudioVideoCapture) {
                    _webView->page()->setFeaturePermission(
                        securityOrigin,
                        feature,
                        QWebEnginePage::PermissionGrantedByUser
                        );
                } else {
                    _webView->page()->setFeaturePermission(
                        securityOrigin,
                        feature,
                        QWebEnginePage::PermissionDeniedByUser
                        );
                }
            });

    _webView->load(QUrl("qrc:/web/rtc_call.html"));
}
