#ifndef BROWSERRTCBRIDGE_H
#define BROWSERRTCBRIDGE_H

#include <QObject>
#include <QString>

class QWebSocketServer;
class QWebSocket;
class QTcpServer;
class QTcpSocket;

class BrowserRtcBridge : public QObject
{
    Q_OBJECT
public:
    static BrowserRtcBridge* GetInstance();

    explicit BrowserRtcBridge(QObject* parent = nullptr);

    bool StartServers();
    void StopServers();

    void OpenBrowserAsCaller();
    void OpenBrowserAsCallee();

    void SendStartCaller();
    void SendStartCallee();

    void SendRemoteOffer(const QString& sdp);
    void SendRemoteAnswer(const QString& sdp);
    void SendRemoteIce(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);
    void SendHangup();

signals:
    void sig_browser_ready();
    void sig_offer_created(const QString& sdp);
    void sig_answer_created(const QString& sdp);
    void sig_ice_candidate(const QString& candidate, const QString& sdpMid, int sdpMLineIndex);
    void sig_connected();
    void sig_hangup();
    void sig_error(const QString& err);

private slots:
    void slot_new_http_connection();
    void slot_new_ws_connection();
    void slot_ws_text_message(const QString& msg);
    void slot_ws_disconnected();

private:
    void SendJsonToBrowser(const QByteArray& json);
    QByteArray BuildHtmlResponse() const;
    QByteArray BuildNotFoundResponse() const;
    QString HtmlPage() const;

private:
    QTcpServer* _httpServer = nullptr;
    QWebSocketServer* _wsServer = nullptr;
    QWebSocket* _browserSocket = nullptr;

    quint16 _httpPort = 19090;
    quint16 _wsPort = 19091;
};
#endif // BROWSERRTCBRIDGE_H
