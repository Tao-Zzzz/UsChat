#include "BrowserRtcBridge.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

BrowserRtcBridge* BrowserRtcBridge::GetInstance()
{
    static BrowserRtcBridge instance;
    return &instance;
}

BrowserRtcBridge::BrowserRtcBridge(QObject* parent)
    : QObject(parent)
{
}

bool BrowserRtcBridge::StartServers()
{
    if (!_httpServer) {
        _httpServer = new QTcpServer(this);
        connect(_httpServer, &QTcpServer::newConnection,
                this, &BrowserRtcBridge::slot_new_http_connection);
    }

    if (!_wsServer) {
        _wsServer = new QWebSocketServer(QStringLiteral("rtc-bridge"),
                                         QWebSocketServer::NonSecureMode,
                                         this);
        connect(_wsServer, &QWebSocketServer::newConnection,
                this, &BrowserRtcBridge::slot_new_ws_connection);
    }

    if (!_httpServer->isListening()) {
        if (!_httpServer->listen(QHostAddress::LocalHost, _httpPort)) {
            emit sig_error(QStringLiteral("本地 HTTP 服务启动失败"));
            return false;
        }
    }

    if (!_wsServer->isListening()) {
        if (!_wsServer->listen(QHostAddress::LocalHost, _wsPort)) {
            emit sig_error(QStringLiteral("本地 WebSocket 服务启动失败"));
            return false;
        }
    }

    return true;
}

void BrowserRtcBridge::StopServers()
{
    if (_browserSocket) {
        _browserSocket->close();
        _browserSocket->deleteLater();
        _browserSocket = nullptr;
    }

    if (_wsServer && _wsServer->isListening()) {
        _wsServer->close();
    }

    if (_httpServer && _httpServer->isListening()) {
        _httpServer->close();
    }
}

void BrowserRtcBridge::OpenBrowserAsCaller()
{
    if (!StartServers()) {
        return;
    }

    QDesktopServices::openUrl(
        QUrl(QString("http://127.0.0.1:%1/rtc_call.html?role=caller").arg(_httpPort))
        );
}

void BrowserRtcBridge::OpenBrowserAsCallee()
{
    if (!StartServers()) {
        return;
    }

    QDesktopServices::openUrl(
        QUrl(QString("http://127.0.0.1:%1/rtc_call.html?role=callee").arg(_httpPort))
        );
}

void BrowserRtcBridge::SendStartCaller()
{
    QJsonObject obj;
    obj["type"] = "start_caller";
    SendJsonToBrowser(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void BrowserRtcBridge::SendStartCallee()
{
    QJsonObject obj;
    obj["type"] = "start_callee";
    SendJsonToBrowser(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void BrowserRtcBridge::SendRemoteOffer(const QString& sdp)
{
    QJsonObject obj;
    obj["type"] = "remote_offer";
    obj["sdp"] = sdp;
    SendJsonToBrowser(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void BrowserRtcBridge::SendRemoteAnswer(const QString& sdp)
{
    QJsonObject obj;
    obj["type"] = "remote_answer";
    obj["sdp"] = sdp;
    SendJsonToBrowser(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void BrowserRtcBridge::SendRemoteIce(const QString& candidate, const QString& sdpMid, int sdpMLineIndex)
{
    QJsonObject obj;
    obj["type"] = "remote_ice";
    obj["candidate"] = candidate;
    obj["sdpMid"] = sdpMid;
    obj["sdpMLineIndex"] = sdpMLineIndex;
    SendJsonToBrowser(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void BrowserRtcBridge::SendHangup()
{
    QJsonObject obj;
    obj["type"] = "hangup";
    SendJsonToBrowser(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void BrowserRtcBridge::slot_new_http_connection()
{
    while (_httpServer->hasPendingConnections()) {
        QTcpSocket* socket = _httpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray req = socket->readAll();

            if (req.startsWith("GET /rtc_call.html")) {
                QByteArray resp = BuildHtmlResponse();
                socket->write(resp);
            } else {
                socket->write(BuildNotFoundResponse());
            }

            socket->disconnectFromHost();
        });
    }
}

void BrowserRtcBridge::slot_new_ws_connection()
{
    QWebSocket* sock = _wsServer->nextPendingConnection();
    if (!sock) {
        return;
    }

    if (_browserSocket) {
        _browserSocket->close();
        _browserSocket->deleteLater();
        _browserSocket = nullptr;
    }

    _browserSocket = sock;

    connect(_browserSocket, &QWebSocket::textMessageReceived,
            this, &BrowserRtcBridge::slot_ws_text_message);

    connect(_browserSocket, &QWebSocket::disconnected,
            this, &BrowserRtcBridge::slot_ws_disconnected);
}

void BrowserRtcBridge::slot_ws_text_message(const QString& msg)
{
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();
    const QString type = obj.value("type").toString();

    if (type == "ready") {
        emit sig_browser_ready();
        return;
    }

    if (type == "offer") {
        emit sig_offer_created(obj.value("sdp").toString());
        return;
    }

    if (type == "answer") {
        emit sig_answer_created(obj.value("sdp").toString());
        return;
    }

    if (type == "ice") {
        emit sig_ice_candidate(
            obj.value("candidate").toString(),
            obj.value("sdpMid").toString(),
            obj.value("sdpMLineIndex").toInt()
            );
        return;
    }

    if (type == "connected") {
        emit sig_connected();
        return;
    }

    if (type == "hangup") {
        emit sig_hangup();
        return;
    }

    if (type == "error") {
        emit sig_error(obj.value("msg").toString());
        return;
    }
}

void BrowserRtcBridge::slot_ws_disconnected()
{
    if (_browserSocket) {
        _browserSocket->deleteLater();
        _browserSocket = nullptr;
    }
}

void BrowserRtcBridge::SendJsonToBrowser(const QByteArray& json)
{
    if (_browserSocket) {
        _browserSocket->sendTextMessage(QString::fromUtf8(json));
    }
}

QByteArray BrowserRtcBridge::BuildHtmlResponse() const
{
    QByteArray body = HtmlPage().toUtf8();
    QByteArray resp;
    resp += "HTTP/1.1 200 OK\r\n";
    resp += "Content-Type: text/html; charset=utf-8\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body;
    return resp;
}

QByteArray BrowserRtcBridge::BuildNotFoundResponse() const
{
    QByteArray body = "404";
    QByteArray resp;
    resp += "HTTP/1.1 404 Not Found\r\n";
    resp += "Content-Type: text/plain\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body;
    return resp;
}

QString BrowserRtcBridge::HtmlPage() const
{
    return QStringLiteral(R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<title>RTC Call</title>
<style>
body { margin:0; background:#111; color:#fff; font-family:sans-serif; }
#root { display:flex; flex-direction:column; height:100vh; }
#top { padding:10px 14px; background:#1c1c1c; }
#videos { flex:1; position:relative; background:#000; }
#remoteVideo { width:100%; height:100%; object-fit:cover; background:#222; }
#localVideo {
  position:absolute; right:16px; top:16px; width:220px; height:150px;
  object-fit:cover; background:#444; border:2px solid rgba(255,255,255,0.2); border-radius:10px;
}
#bottom { padding:12px; display:flex; gap:10px; justify-content:center; background:#1c1c1c; }
button { padding:10px 18px; border:none; border-radius:8px; cursor:pointer; }
#hangupBtn { background:#d9534f; color:white; }
</style>
</head>
<body>
<div id="root">
  <div id="top">
    <span id="status">初始化中...</span>
  </div>
  <div id="videos">
    <video id="remoteVideo" autoplay playsinline></video>
    <video id="localVideo" autoplay playsinline muted></video>
  </div>
  <div id="bottom">
    <button id="hangupBtn">挂断</button>
  </div>
</div>

<script>
(() => {
  const statusEl = document.getElementById('status');
  const localVideo = document.getElementById('localVideo');
  const remoteVideo = document.getElementById('remoteVideo');
  const hangupBtn = document.getElementById('hangupBtn');

  const ws = new WebSocket('ws://127.0.0.1:19091');
  let pc = null;
  let localStream = null;
  let started = false;

  function setStatus(t) {
    statusEl.textContent = t;
  }

  function send(obj) {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    }
  }

  async function ensureMedia() {
    if (localStream) return localStream;
    localStream = await navigator.mediaDevices.getUserMedia({ video: true, audio: true });
    localVideo.srcObject = localStream;
    return localStream;
  }

  async function ensurePc() {
    if (pc) return pc;

    pc = new RTCPeerConnection({
      iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
    });

    pc.onicecandidate = (e) => {
      if (!e.candidate) return;
      send({
        type: 'ice',
        candidate: e.candidate.candidate,
        sdpMid: e.candidate.sdpMid,
        sdpMLineIndex: e.candidate.sdpMLineIndex
      });
    };

    pc.ontrack = (e) => {
      if (e.streams && e.streams[0]) {
        remoteVideo.srcObject = e.streams[0];
      }
    };

    pc.onconnectionstatechange = () => {
      setStatus('连接状态: ' + pc.connectionState);
      if (pc.connectionState === 'connected') {
        send({ type: 'connected' });
      }
      if (pc.connectionState === 'disconnected' ||
          pc.connectionState === 'failed' ||
          pc.connectionState === 'closed') {
        // 不自动发 hangup，避免重复
      }
    };

    const stream = await ensureMedia();
    stream.getTracks().forEach(track => pc.addTrack(track, stream));

    return pc;
  }

  async function startCaller() {
    if (started) return;
    started = true;
    setStatus('正在创建 Offer...');
    const peer = await ensurePc();
    const offer = await peer.createOffer();
    await peer.setLocalDescription(offer);
    send({ type: 'offer', sdp: offer.sdp });
    setStatus('Offer 已发送，等待对方...');
  }

  async function startCallee() {
    if (started) return;
    started = true;
    setStatus('等待主叫 Offer...');
    await ensurePc();
  }

  async function onRemoteOffer(sdp) {
    const peer = await ensurePc();
    await peer.setRemoteDescription({ type: 'offer', sdp });
    const answer = await peer.createAnswer();
    await peer.setLocalDescription(answer);
    send({ type: 'answer', sdp: answer.sdp });
    setStatus('Answer 已发送');
  }

  async function onRemoteAnswer(sdp) {
    const peer = await ensurePc();
    await peer.setRemoteDescription({ type: 'answer', sdp });
    setStatus('远端 Answer 已设置');
  }

  async function onRemoteIce(candidate, sdpMid, sdpMLineIndex) {
    const peer = await ensurePc();
    try {
      await peer.addIceCandidate({ candidate, sdpMid, sdpMLineIndex });
    } catch (e) {
      console.error(e);
    }
  }

  function doHangup(sendSignal = true) {
    if (sendSignal) send({ type: 'hangup' });

    if (pc) {
      pc.getSenders().forEach(s => {
        if (s.track) s.track.stop();
      });
      pc.close();
      pc = null;
    }

    if (localStream) {
      localStream.getTracks().forEach(t => t.stop());
      localStream = null;
    }

    remoteVideo.srcObject = null;
    localVideo.srcObject = null;
    started = false;
    setStatus('通话已结束');
  }

  ws.onopen = () => {
    setStatus('浏览器已连接本地桥');
    send({ type: 'ready' });
  };

  ws.onmessage = async (event) => {
    const msg = JSON.parse(event.data);

    try {
      switch (msg.type) {
        case 'start_caller':
          await startCaller();
          break;
        case 'start_callee':
          await startCallee();
          break;
        case 'remote_offer':
          await onRemoteOffer(msg.sdp);
          break;
        case 'remote_answer':
          await onRemoteAnswer(msg.sdp);
          break;
        case 'remote_ice':
          await onRemoteIce(msg.candidate, msg.sdpMid, msg.sdpMLineIndex);
          break;
        case 'hangup':
          doHangup(false);
          break;
      }
    } catch (e) {
      console.error(e);
      send({ type: 'error', msg: String(e) });
      setStatus('错误: ' + e);
    }
  };

  hangupBtn.onclick = () => doHangup(true);
  window.addEventListener('beforeunload', () => doHangup(true));
})();
</script>
</body>
</html>
)HTML");
}
