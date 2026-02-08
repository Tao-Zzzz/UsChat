#include "aimgr.h"
#include "httpmgr.h"
#include "usermgr.h"

AIMgr::AIMgr()
{
    connect(
        HttpMgr::GetInstance().get(),
        &HttpMgr::sig_http_finish,
        this,
        &AIMgr::slot_http_finish
        );
}

void AIMgr::LoadAIThreads()
{
    QJsonObject params;
    params["uid"] = UserMgr::GetInstance()->GetUid();

    HttpMgr::GetInstance()->GetHttpReq(
        QUrl("http://127.0.0.1:8000/ai/load_thread"),
        params,
        ReqId::AI_LOAD_THREAD_REQ,
        Modules::AIMOD
        );
}

void AIMgr::SetAIThreads(QJsonArray arr)
{
    for (const QJsonValue &value : arr) {
        if (!value.isObject()) continue;

        QJsonObject obj = value.toObject();

        int threadId    = obj.value("ai_thread_id").toInt(-1);
        QString title   = obj.value("title").toString();
        QString updated = obj.value("updated_at").toString();

        qDebug() << "Thread ID:" << threadId
                 << "Title:" << title
                 << "Updated:" << updated;

        _ai_thread_ids.push_back(threadId);
        _ai_thread_datas[threadId] = make_shared<AIThreadData>(title, updated);
        // 你可以在这里存到 QList、模型、数据库等

    }
}


void AIMgr::slot_http_finish(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) return;

    if (id == ReqId::AI_LOAD_THREAD_REQ) {
        QJsonDocument doc = QJsonDocument::fromJson(res.toUtf8());
        QJsonArray arr = doc.array();

        SetAIThreads(arr);

        //设置好了然后...
        //emit sig_ai_threads_loaded();
    }

    if (id == ReqId::AI_LOAD_CHAT_REQ) {
        // 同理
    }
}
