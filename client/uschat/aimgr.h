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

Q_DECLARE_METATYPE(std::shared_ptr<AIThreadData>)


class AIMgr : public QObject, public Singleton<AIMgr>,
              public std::enable_shared_from_this<AIMgr> {
    Q_OBJECT
public:
    void LoadAIThreads();
    void LoadAIChatMsgs(int ai_thread_id);
    void SetAIThreads(QJsonArray arr);
    ~ AIMgr();
    friend class Singleton<AIMgr>;
private:
    AIMgr();

    int current_active_ai_thread_id = -1;
    std::vector<int> _ai_thread_ids;
    QMap<int, std::shared_ptr<AIThreadData>> _ai_thread_datas;
    // ...

signals:
    void sig_ai_threads_loaded();
    void sig_ai_chat_loaded(int ai_thread_id);

private slots:
    void slot_http_finish(ReqId id, QString res, ErrorCodes err);
};


#endif // AIMGR_H
