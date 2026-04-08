#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include "userdata.h"
#include <QMap>
#include <QHash>
#include <QList>
#include <QSet>
#include "chatitembase.h"
#include "moremenu.h"

class EmojiMenu;
class QLabel;

namespace Ui {
class ChatPage;
}

class ChatPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();

    void SetChatData(std::shared_ptr<ChatThreadData> chat_data);
    void AppendChatMsg(std::shared_ptr<ChatDataBase> msg, bool rsp = true);
    void UpdateChatStatus(std::shared_ptr<ChatDataBase> msg);
    void UpdateImgChatStatus(std::shared_ptr<ImgChatData> img_msg);
    void SetSelfIcon(ChatItemBase* pChatItem, QString icon);
    void UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info);
    void UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info , QString file_path);
    void send_msg_to_ai();
    void AppendAiChatMsg(std::shared_ptr<TextChatData> msg);
    void UpdateAiChatStatus(QString unique_id, int msg_id);
    void showAiHistoryWindow();
    void showAiModelWindow();
    void clearItems();
    void AppendOtherMsg(std::shared_ptr<ChatDataBase> msg);
    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);
    void DownloadFileFinished(std::shared_ptr<MsgInfo> msg_info, QString file_path);
    void UpdateImgChatFinshStatusById(int msg_id);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void on_send_btn_clicked();
    void on_receive_btn_clicked();
    void on_clicked_paused(QString unique_name, TransferType transfer_type);
    void on_clicked_resume(QString unique_name, TransferType transfer_type);
    void slot_clicked_more_label(QString name, ClickLbState state);
    void slot_clicked_emoji_label(QString name, ClickLbState state);
    void slot_insert_emoji(const QString& token);
    void slot_ai_history_selected(int ai_thread_id);
    void slot_ai_model_selected(int ai_model_id);

private:
    struct ThreadUiCache
    {
        QList<ChatItemBase*> items;
        QHash<QString, ChatItemBase*> unrspItemMap;
        QHash<qint64, ChatItemBase*> baseItemMap;
        bool valid = false;
    };

    void UpdateTitle(std::shared_ptr<ChatThreadData> chat_data);
    void BuildChatItemsFromData(std::shared_ptr<ChatThreadData> chat_data);
    void SaveCurrentThreadCache();
    bool RestoreThreadCache(int thread_id);
    void ClearCurrentUiOnly();
    void ClearThreadCache(int thread_id);
    void ClearAllThreadCache();
    void showEmojiMenu();
    void hideEmojiMenu();

private:
    Ui::ChatPage *ui;
    std::shared_ptr<ChatThreadData> _chat_data;

    QMap<QString, QWidget*>  _bubble_map;
    QHash<QString, ChatItemBase*> _unrsp_item_map;
    QHash<qint64, ChatItemBase*> _base_item_map;

    MoreMenu* _more_menu = nullptr;
    EmojiMenu* _emoji_menu = nullptr;

    QHash<int, ThreadUiCache> _thread_ui_cache;
    int _current_thread_id = -1;

signals:
    void sig_request_load_ai_history(int ai_thread_id);
    void sig_request_change_ai_model(int ai_model_id);
};

#endif // CHATPAGE_H
