#include "aimgr.h"
#include "httpmgr.h"
#include "usermgr.h"

AIMgr::AIMgr()
{
    connect(
        HttpMgr::GetInstance().get(),
        &HttpMgr::sig_ai_mod_finish,
        this,
        &AIMgr::slot_http_finish
        );
}

void AIMgr::LoadAIThreads()
{

    int uid = UserMgr::GetInstance()->GetUid();
    qDebug() << "Sending uid to AiServer:" << uid;

    QJsonObject params;
    params["uid"] = uid;


    qDebug() << "[UID] After insert into params:" << params["uid"].toInt()
             << "| isNull:" << params["uid"].isNull()
             << "| isUndefined:" << params["uid"].isUndefined();

    QUrl url;
    url.setScheme(_ai_scheme);
    url.setHost(_ai_host);
    url.setPort(_ai_port);
    url.setPath(ApiPath::LoadThread);

    HttpMgr::GetInstance()->GetHttpReq(
        url,
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

void AIMgr::SetAIModels(QJsonArray arr)
{
    _ai_model_ids.clear();
    _ai_models_name.clear();

    for (const QJsonValue &value : arr)
    {
        if (!value.isObject())
            continue;

        QJsonObject obj = value.toObject();

        int id = obj.value("id").toInt(-1);
        QString name = obj.value("name").toString();

        if (id < 0)
            continue;

        _ai_model_ids.push_back(id);
        _ai_models_name[id] = name;

        qDebug() << "[Model] id:" << id
                 << "name:" << name;
    }
}


AIMgr::~AIMgr()
{

}

int AIMgr::GetCurAiThread()
{
    return _current_active_ai_thread_id;
}

void AIMgr::SetCurAiThread(int thread_id)
{
    _current_active_ai_thread_id = thread_id;
}


void AIMgr::slot_http_finish(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) return;

    if (id == ReqId::AI_LOAD_THREAD_REQ) {
        QJsonDocument doc = QJsonDocument::fromJson(res.toUtf8());
        QJsonObject rootObj = doc.object();

        QJsonArray threadsArr = rootObj.value("threads").toArray();
        QJsonArray modelsArr  = rootObj.value("models").toArray();

        SetAIThreads(threadsArr);
        SetAIModels(modelsArr);

        //设置好了然后...
        //emit sig_ai_threads_loaded();
    }

    if (id == ReqId::AI_LOAD_CHAT_REQ) {
        // 同理
    }
}

QMap<int, std::shared_ptr<AIThreadData>> AIMgr::GetAllAiHistoryChat(){
    return _ai_thread_datas;
}

QMap<int, QString> AIMgr::GetAllAiModel(){
    return _ai_models_name;
}


void AIMgr::SetAIHost(QString host)
{
    _ai_host = host;
}

void AIMgr::SetAIScheme(QString scheme)
{
    _ai_scheme = scheme;
}

void AIMgr::SetAIPort(int port)
{
    _ai_port = port;
}

QString AIMgr::GetAIModelName(int model_id)
{
    return _ai_models_name[model_id];
}

QString AIMgr::GetCurAiModelName()
{
    if(_current_active_ai_model_id == -1){
        return "Qwen3-8B";
    }

    return _ai_models_name[_current_active_ai_model_id];
}

void AIMgr::SetCurAiModel(int model_id)
{
    _current_active_ai_model_id = model_id;
}


QString AIMgr::GetAIHost()
{
    return _ai_host;
}

QString AIMgr::GetAIScheme()
{
    return _ai_scheme;
}

int AIMgr::GetAiPort()
{
    return _ai_port;
}
