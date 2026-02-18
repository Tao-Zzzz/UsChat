#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include "userdata.h"
#include <QMap>
#include "chatitembase.h"
#include "moremenu.h"

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
    void AppendChatMsg(std::shared_ptr<ChatDataBase> msg);
    void UpdateChatStatus(std::shared_ptr<ChatDataBase> msg);
    void UpdateImgChatStatus(std::shared_ptr<ImgChatData> img_msg);
    void SetSelfIcon(ChatItemBase* pChatItem, QString icon);
    void UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info);
    void send_msg_to_ai();
    void AppendAiChatMsg(std::shared_ptr<TextChatData> msg);
    void UpdateAiChatStatus(QString unique_id, int msg_id);
    void showAiHistoryWindow();
    void showAiModelWindow();
    void clearItems();
protected:
    void paintEvent(QPaintEvent *event);

private slots:
    void on_send_btn_clicked();

    void on_receive_btn_clicked();

    //PictureBubble
    void on_clicked_paused(QString unique_name, TransferType transfer_type);
    //PictureBubble
    void on_clicked_resume(QString unique_name, TransferType transfer_type);
    void slot_clicked_more_label(QString name, ClickLbState state);
    void slot_ai_history_selected(int ai_thread_id);
    void slot_ai_model_selected(int ai_model_id);
private:

    Ui::ChatPage *ui;
    std::shared_ptr<ChatThreadData> _chat_data;
    QMap<QString, QWidget*>  _bubble_map;
    //¦Ä
    QHash<QString, ChatItemBase*> _unrsp_item_map;
    //
    QHash<qint64, ChatItemBase*> _base_item_map;

    MoreMenu* _more_menu = nullptr;

signals:
    void sig_request_load_ai_history(int ai_thread_id);
    void sig_request_change_ai_model(int ai_model_id);
};

#endif // CHATPAGE_H
