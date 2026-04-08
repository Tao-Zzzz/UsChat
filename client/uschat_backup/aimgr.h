#ifndef AIMGR_H
#define AIMGR_H

#include <memory>
#include <singleton.h>

struct AIThreadData{
    QString _title;
    QString _last_updated_time;
    AIThreadData(QString title, QString last_updated_time){
        _title = title;
        _last_updated_time = last_updated_time;
    }
};

struct AIModelData
{
    int id;
    QString name;
};


Q_DECLARE_METATYPE(std::shared_ptr<AIModelData>)
Q_DECLARE_METATYPE(std::shared_ptr<AIThreadData>)


class AIMgr : public QObject, public Singleton<AIMgr>,
              public std::enable_shared_from_this<AIMgr> {
    Q_OBJECT
public:
    void LoadAIThreads();
    void LoadAIChatMsgs(int ai_thread_id);
    void SetAIThreads(QJsonArray arr);
    void SetAIModels(QJsonArray arr);
    ~ AIMgr();
    friend class Singleton<AIMgr>;
    int GetCurAiThread();
    void SetCurAiThread(int thread_id);
    QMap<int, std::shared_ptr<AIThreadData>> GetAllAiHistoryChat();
    QString GetAIHost();
    QString GetAIScheme();
    int GetAiPort();
    void SetAIHost(QString host);
    void SetAIScheme(QString scheme);
    void SetAIPort(int port);

    QString GetAIModelName(int model_id);

    QString GetCurAiModelName();
    void SetCurAiModel(int model_id);

    QMap<int, QString> GetAllAiModel();
private:
    AIMgr();

    int _current_active_ai_thread_id = -1;
    std::vector<int> _ai_thread_ids;
    QMap<int, std::shared_ptr<AIThreadData>> _ai_thread_datas;
    // ...

    QString _ai_host;
    QString _ai_scheme;
    int _ai_port;

    std::vector<int> _ai_model_ids;
    QMap<int, QString> _ai_models_name;
    int _current_active_ai_model_id = -1;

signals:
    void sig_ai_threads_loaded();
    void sig_ai_chat_loaded(int ai_thread_id);

private slots:
    void slot_http_finish(ReqId id, QString res, ErrorCodes err);
};


#endif // AIMGR_H
