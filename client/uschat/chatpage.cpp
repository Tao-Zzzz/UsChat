#include "chatpage.h"
#include "ui_chatpage.h"
#include <QStyleOption>
#include <QPainter>
#include "ChatItemBase.h"
#include "TextBubble.h"
#include "PictureBubble.h"
#include "applyfrienditem.h"
#include "usermgr.h"
#include <QJsonArray>
#include <QJsonObject>
#include "tcpmgr.h"
#include <QUuid>
#include <QStandardPaths>
#include "filetcpmgr.h"
#include "httpmgr.h"
#include "aimgr.h"
#include "aihistorydialog.h"
#include "moremenu.h"
#include "aimodeldialog.h"
#include "EmojiMenu.h"
#include <QTextCursor>


ChatPage::ChatPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatPage)
{
    ui->setupUi(this);

    ui->receive_btn->SetState("normal","hover","press");
    ui->send_btn->SetState("normal","hover","press");

    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");
    ui->more_lb->SetState("normal","hover","press","normal","hover","press");

    connect(ui->more_lb, &ClickedLabel::clicked,
            this, &ChatPage::slot_clicked_more_label);

    connect(ui->emo_lb, &ClickedLabel::clicked,
            this, &ChatPage::slot_clicked_emoji_label);
}


ChatPage::~ChatPage()
{
    if (_emoji_menu) {
        _emoji_menu->hide();
        delete _emoji_menu;
        _emoji_menu = nullptr;
    }

    ClearCurrentUiOnly();
    ClearAllThreadCache();
    delete ui;
}

void ChatPage::UpdateTitle(std::shared_ptr<ChatThreadData> chat_data)
{
    if (!chat_data) {
        ui->title_lb->clear();
        return;
    }

    auto other_id = chat_data->GetOtherId();

    if (other_id == -1) {
        QString model_name = AIMgr::GetInstance()->GetCurAiModelName();
        if (model_name.isEmpty()) {
            ui->title_lb->setText("AI");
        } else {
            ui->title_lb->setText("AI " + model_name);
        }
        return;
    }

    if (other_id == 0) {
        ui->title_lb->setText(chat_data->GetGroupName());
        return;
    }

    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);
    if (friend_info != nullptr) {
        ui->title_lb->setText(friend_info->_name);
    }
}

void ChatPage::ClearCurrentUiOnly()
{
    ui->chat_data_list->beginBatchUpdate();
    ui->chat_data_list->removeAllItem();
    ui->chat_data_list->endBatchUpdate(false);

    _unrsp_item_map.clear();
    _base_item_map.clear();
}

void ChatPage::ClearThreadCache(int thread_id)
{
    auto it = _thread_ui_cache.find(thread_id);
    if (it == _thread_ui_cache.end()) {
        return;
    }

    for (ChatItemBase* item : it.value().items) {
        delete item;
    }

    _thread_ui_cache.erase(it);
}

void ChatPage::ClearAllThreadCache()
{
    QSet<ChatItemBase*> deletedSet;
    for (auto it = _thread_ui_cache.begin(); it != _thread_ui_cache.end(); ++it) {
        for (ChatItemBase* item : it.value().items) {
            if (item && !deletedSet.contains(item)) {
                deletedSet.insert(item);
                delete item;
            }
        }
    }
    _thread_ui_cache.clear();
}

void ChatPage::SaveCurrentThreadCache()
{
    // if (_current_thread_id < 0) {
    //     ui->chat_data_list->beginBatchUpdate();
    //     ui->chat_data_list->clearLayoutItemsOnly();
    //     ui->chat_data_list->endBatchUpdate(false);

    //     _unrsp_item_map.clear();
    //     _base_item_map.clear();
    //     return;
    // }

    ThreadUiCache cache;

    ui->chat_data_list->beginBatchUpdate();
    QList<QWidget*> widgets = ui->chat_data_list->takeAllItems();
    ui->chat_data_list->endBatchUpdate(false);

    for (QWidget* w : widgets) {
        ChatItemBase* item = qobject_cast<ChatItemBase*>(w);
        if (item) {
            cache.items.append(item);
        }
    }

    cache.unrspItemMap = _unrsp_item_map;
    cache.baseItemMap = _base_item_map;
    cache.valid = true;

    _thread_ui_cache[_current_thread_id] = cache;

    _unrsp_item_map.clear();
    _base_item_map.clear();
}

bool ChatPage::RestoreThreadCache(int thread_id)
{
    auto it = _thread_ui_cache.find(thread_id);
    if (it == _thread_ui_cache.end() || !it.value().valid) {
        return false;
    }

    QList<QWidget*> widgets;
    for (ChatItemBase* item : it.value().items) {
        if (item) {
            widgets.append(item);
        }
    }

    ui->chat_data_list->beginBatchUpdate();
    ui->chat_data_list->setChatItems(widgets);
    ui->chat_data_list->endBatchUpdate(true);

    _unrsp_item_map = it.value().unrspItemMap;
    _base_item_map = it.value().baseItemMap;
    return true;
}

void ChatPage::BuildChatItemsFromData(std::shared_ptr<ChatThreadData> chat_data)
{
    ui->chat_data_list->beginBatchUpdate();

    ui->chat_data_list->clearLayoutItemsOnly();
    _unrsp_item_map.clear();
    _base_item_map.clear();

    if (!chat_data) {
        ui->chat_data_list->endBatchUpdate(false);
        return;
    }

    auto other_id = chat_data->GetOtherId();

    if (other_id == -1) {
        for (auto& msg : chat_data->GetMsgMapRef()) {
            auto text_msg = std::dynamic_pointer_cast<TextChatData>(msg);
            if (text_msg) {
                AppendAiChatMsg(text_msg);
            }
        }

        for (auto& msg : chat_data->GetMsgUnRspRef()) {
            auto text_msg = std::dynamic_pointer_cast<TextChatData>(msg);
            if (text_msg) {
                AppendAiChatMsg(text_msg);
            }
        }

        ui->chat_data_list->endBatchUpdate(true);
        return;
    }

    for (auto& msg : chat_data->GetMsgMapRef()) {
        AppendChatMsg(msg);
    }

    for (auto& msg : chat_data->GetMsgUnRspRef()) {
        AppendChatMsg(msg, false);
    }

    ui->chat_data_list->endBatchUpdate(true);
}

void ChatPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data)
{
    if (!chat_data) {
        return;
    }

    const int new_thread_id = chat_data->GetThreadId();

    // 同一会话刷新
    if (_chat_data && _chat_data->GetThreadId() == new_thread_id) {
        _chat_data = chat_data;
        _current_thread_id = new_thread_id;
        UpdateTitle(chat_data);

        ClearThreadCache(new_thread_id);
        BuildChatItemsFromData(chat_data);
        return;
    }

    // 先缓存当前会话
    SaveCurrentThreadCache();

    _chat_data = chat_data;
    _current_thread_id = new_thread_id;
    UpdateTitle(chat_data);

    // 优先恢复缓存
    if (RestoreThreadCache(new_thread_id)) {
        return;
    }

    // 没缓存就首次构建
    BuildChatItemsFromData(chat_data);
}

void ChatPage::AppendChatMsg(std::shared_ptr<ChatDataBase> msg, bool rsp)
{
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role;

    if (msg->GetSendUid() == self_info->_uid) {
        role = ChatRole::Self;
        ChatItemBase* pChatItem = new ChatItemBase(role);

        pChatItem->setUserName(self_info->_name);
        SetSelfIcon(pChatItem, self_info->_icon);
        QWidget* pBubble = nullptr;

        qDebug() << "msg type:" << (int)msg->GetMsgType();
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        } else if (msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = dynamic_pointer_cast<ImgChatData>(msg);
            qDebug() << "构造气泡时的路径:" << img_msg->_msg_info->_preview_pix;
            auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;

            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }

        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);

        if (rsp) {
            _base_item_map[msg->GetMsgId()] = pChatItem;
        } else {
            _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
        }
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());
        if (friend_info == nullptr) {
            return;
        }
        pChatItem->setUserName(friend_info->_name);

        QRegularExpression regex("^:/res/head_(\\d+)\\.jpg$");
        QRegularExpressionMatch match = regex.match(friend_info->_icon);
        if (match.hasMatch()) {
            pChatItem->setUserIcon(QPixmap(friend_info->_icon));
        }
        else {
            QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir avatarsDir(storageDir + "/user/" + QString::number(msg->GetSendUid()) + "/avatars");
            if (avatarsDir.exists()) {
                QString avatarPath = avatarsDir.filePath(friend_info->_icon);
                QPixmap pixmap(avatarPath);
                if (!pixmap.isNull()) {
                    pChatItem->setUserIcon(pixmap);
                }
                else {
                    qWarning() << "无法加载上传的头像：" << avatarPath;
                    auto icon_label = pChatItem->getIconLabel();
                    LoadHeadIcon(avatarPath, icon_label, friend_info->_icon,"other_icon");
                }
            }
            else {
                qWarning() << "头像存储目录不存在：" << avatarsDir.path();
                avatarsDir.mkpath(".");
                auto icon_label = pChatItem->getIconLabel();
                QString avatarPath = avatarsDir.filePath(friend_info->_icon);
                LoadHeadIcon(avatarPath, icon_label, friend_info->_icon, "other_icon");
            }
        }

        QWidget* pBubble = nullptr;
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        if (msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = dynamic_pointer_cast<ImgChatData>(msg);
            auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;

            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }

        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);

        if (rsp) {
            _base_item_map[msg->GetMsgId()] = pChatItem;
        } else {
            _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
        }
    }
}

void ChatPage::UpdateChatStatus(std::shared_ptr<ChatDataBase> msg)
{
    auto iter = _unrsp_item_map.find(msg->GetUniqueId());
    if (iter == _unrsp_item_map.end()) {
        return;
    }

    iter.value()->setStatus(msg->GetStatus());
    _base_item_map[msg->GetMsgId()] = iter.value();
    _unrsp_item_map.erase(iter);
}

void ChatPage::UpdateImgChatStatus(std::shared_ptr<ImgChatData> msg)
{
    auto iter = _unrsp_item_map.find(msg->GetUniqueId());
    if (iter == _unrsp_item_map.end()) {
        return;
    }

    iter.value()->setStatus(msg->GetStatus());
    _base_item_map[msg->GetMsgId()] = iter.value();
    _unrsp_item_map.erase(iter);

    auto bubble = _base_item_map[msg->GetMsgId()]->getBubble();
    PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
    pic_bubble->setMsgInfo(msg->_msg_info);
}

void ChatPage::UpdateImgChatFinshStatusById(int msg_id)
{
    auto iter = _base_item_map.find(msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    qDebug() << "修改图片状态为2";
    iter.value()->setStatus(MsgStatus::READED);
    _base_item_map[msg_id] = iter.value();
}

void ChatPage::UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info)
{
    auto iter = _base_item_map.find(msg_info->_msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        auto bubble = iter.value()->getBubble();
        PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        pic_bubble->setProgress(msg_info->_rsp_size, msg_info->_total_size);
    }
}

void ChatPage::UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info, QString file_path)
{
    auto iter = _base_item_map.find(msg_info->_msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        auto bubble = iter.value()->getBubble();
        PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        pic_bubble->setProgress(msg_info->_rsp_size, msg_info->_total_size);
        pic_bubble->setUploadFinish(file_path);

        auto chat_data_base = _chat_data->GetChatDataBase(msg_info->_msg_id);
        if (chat_data_base == nullptr) {
            return;
        }

        auto img_data = dynamic_pointer_cast<ImgChatData>(chat_data_base);
        img_data->_msg_info->_preview_pix = QPixmap(file_path);
    }
}

void ChatPage::SetSelfIcon(ChatItemBase* pChatItem, QString icon)
{
    QRegularExpression regex("^:/res/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(icon);
    if (match.hasMatch()) {
        pChatItem->setUserIcon(QPixmap(icon));
    }
    else {
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        auto uid = UserMgr::GetInstance()->GetUid();
        QDir avatarsDir(storageDir + "/user/" + QString::number(uid) + "/avatars");
        auto file_name = QFileInfo(icon).fileName();
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name);
            QPixmap pixmap(avatarPath);
            if (!pixmap.isNull()) {
                pChatItem->setUserIcon(pixmap);
            }
            else {
                qWarning() << "无法加载上传的头像：" << avatarPath;
                auto icon_label = pChatItem->getIconLabel();
                LoadHeadIcon(avatarPath, icon_label, file_name, "self_icon");
            }
        }
        else {
            qWarning() << "头像存储目录不存在：" << avatarsDir.path();
            avatarsDir.mkpath(".");
            auto icon_label = pChatItem->getIconLabel();
            QString avatarPath = avatarsDir.filePath(file_name);
            LoadHeadIcon(avatarPath, icon_label, file_name, "self_icon");
        }
    }
}

void ChatPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatPage::AppendAiChatMsg(std::shared_ptr<TextChatData> msg)
{
    ChatRole role;
    if (msg->GetSendUid() == AI_UID) {
        role = ChatRole::Other;
    } else {
        role = ChatRole::Self;
    }

    ChatItemBase* pChatItem = new ChatItemBase(role);

    if (msg->GetSendUid() != AI_UID) {
        auto self_info = UserMgr::GetInstance()->GetUserInfo();
        pChatItem->setUserName(self_info->_name);
        SetSelfIcon(pChatItem, self_info->_icon);
    } else {
        pChatItem->setUserName("AI");
        auto icon_label = pChatItem->getIconLabel();
        QPixmap pixmap(":/res/head_2.jpg");
        QPixmap scaledPixmap = pixmap.scaled(icon_label->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
        icon_label->setPixmap(scaledPixmap);
        icon_label->setScaledContents(true);
    }

    QWidget* pBubble = new TextBubble(role, msg->GetContent());
    pChatItem->setWidget(pBubble);
    pChatItem->setStatus(msg->GetStatus());

    ui->chat_data_list->appendChatItem(pChatItem);
    _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
}

void ChatPage::UpdateAiChatStatus(QString unique_id, int msg_id)
{
    auto iter = _unrsp_item_map.find(unique_id);
    if (iter == _unrsp_item_map.end()) {
        return;
    }

    iter.value()->setStatus(MsgStatus::READED);
    _base_item_map[msg_id] = iter.value();
    _unrsp_item_map.erase(iter);
}

void ChatPage::send_msg_to_ai()
{
    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    auto pTextEdit = ui->chatEdit;

    const QVector<std::shared_ptr<MsgInfo>>& msgList = pTextEdit->getMsgList();
    if (msgList.isEmpty()) return;

    auto msg = msgList[0];
    if (msg->_msg_type != MsgType::TEXT_MSG) {
        qDebug() << "AI only supports text currently";
        return;
    }

    QString content = msg->_text_or_url;
    if (content.isEmpty()) return;

    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(AI_THREAD);

    QUuid uuid = QUuid::createUuid();
    QString uuidString = uuid.toString();

    auto txt_msg = std::make_shared<TextChatData>(uuidString, AI_THREAD, ChatFormType::AI,
                                                  ChatMsgType::TEXT, content, user_info->_uid, 0);

    AppendAiChatMsg(txt_msg);
    chat_data->AppendUnRspMsg(uuidString, txt_msg);

    QJsonObject req;
    req["uid"] = user_info->_uid;
    req["content"] = content;
    req["model"] = AIMgr::GetInstance()->GetCurAiModelName();
    req["unique_id"] = uuidString;
    req["ai_thread_id"] = AIMgr::GetInstance()->GetCurAiThread();

    QUrl url("http://127.0.0.1:8070/ai/chat");
    HttpMgr::GetInstance()->PostHttpReq(
        url,
        req,
        ReqId::ID_AI_CHAT_REQ,
        Modules::AIMOD
        );

    pTextEdit->clear();
}

void ChatPage::on_send_btn_clicked()
{
    if (_chat_data == nullptr) {
        qDebug() << "thread chat info is empty";
        return;
    }

    if (_chat_data->GetOtherId() == -1) {
        send_msg_to_ai();
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Self;
    QString userName = user_info->_name;

    const QVector<std::shared_ptr<MsgInfo>>& msgList = pTextEdit->getMsgList();
    QJsonObject textObj;
    QJsonArray textArray;
    int txt_size = 0;
    auto thread_id = _chat_data->GetThreadId();

    for (int i = 0; i < msgList.size(); ++i)
    {
        if (msgList[i]->_text_or_url.length() > 1024) {
            continue;
        }

        MsgType type = msgList[i]->_msg_type;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        SetSelfIcon(pChatItem, user_info->_icon);
        QWidget* pBubble = nullptr;

        QUuid uuid = QUuid::createUuid();
        QString uuidString = uuid.toString();

        if (type == MsgType::TEXT_MSG)
        {
            int other_id = _chat_data->GetOtherId();
            pBubble = new TextBubble(role, msgList[i]->_text_or_url);

            if (txt_size + msgList[i]->_text_or_url.length() > 1024) {
                textObj["fromuid"] = user_info->_uid;
                textObj["thread_id"] = thread_id;
                textObj["text_array"] = textArray;
                textObj["touid"] = other_id;

                if (other_id == 0) {
                    QJsonArray toMembers;
                    std::vector<int> members = _chat_data->GetGroupMemberUids();
                    for (int m_id : members) {
                        if (m_id == user_info->_uid) continue;
                        toMembers.append(m_id);
                    }
                    textObj["to_member_uids"] = toMembers;
                }

                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();

                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            txt_size += msgList[i]->_text_or_url.length();
            QJsonObject obj;
            QByteArray utf8Message = msgList[i]->_text_or_url.toUtf8();
            auto content = QString::fromUtf8(utf8Message);
            obj["content"] = content;
            obj["unique_id"] = uuidString;
            textArray.append(obj);

            ChatFormType chat_type;
            if (other_id == 0) {
                chat_type = ChatFormType::GROUP;
            } else {
                chat_type = ChatFormType::PRIVATE;
            }

            auto txt_msg = std::make_shared<TextChatData>(
                uuidString, thread_id, chat_type,
                ChatMsgType::TEXT, content, user_info->_uid, 0);

            _chat_data->AppendUnRspMsg(uuidString, txt_msg);
        }
        else if (type == MsgType::IMG_MSG)
        {
            int other_id = _chat_data->GetOtherId();

            if (txt_size) {
                textObj["fromuid"] = user_info->_uid;
                textObj["thread_id"] = thread_id;
                textObj["text_array"] = textArray;
                textObj["touid"] = other_id;

                if (other_id == 0) {
                    QJsonArray toMembers;
                    std::vector<int> members = _chat_data->GetGroupMemberUids();
                    for (int m_id : members) {
                        if (m_id == user_info->_uid) continue;
                        toMembers.append(m_id);
                    }
                    textObj["to_member_uids"] = toMembers;
                }

                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();

                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            pBubble = new PictureBubble(QPixmap(msgList[i]->_text_or_url), role, msgList[i]->_total_size);

            ChatFormType chat_type;
            if (other_id == 0) {
                chat_type = ChatFormType::GROUP;
            } else {
                chat_type = ChatFormType::PRIVATE;
            }

            auto img_msg = std::make_shared<ImgChatData>(
                msgList[i], uuidString, thread_id, chat_type,
                ChatMsgType::PIC, user_info->_uid, 0);

            _chat_data->AppendUnRspMsg(uuidString, img_msg);

            textObj["fromuid"] = user_info->_uid;
            textObj["thread_id"] = thread_id;
            textObj["md5"] = msgList[i]->_md5;
            textObj["name"] = msgList[i]->_unique_name;
            textObj["token"] = UserMgr::GetInstance()->GetToken();
            textObj["unique_id"] = uuidString;
            textObj["text_or_url"] = msgList[i]->_text_or_url;
            textObj["touid"] = other_id;

            if (other_id == 0) {
                QJsonArray toMembers;
                std::vector<int> members = _chat_data->GetGroupMemberUids();
                for (int m_id : members) {
                    if (m_id == user_info->_uid) continue;
                    toMembers.append(m_id);
                }
                textObj["to_member_uids"] = toMembers;
            }

            UserMgr::GetInstance()->AddTransFile(msgList[i]->_unique_name, msgList[i]);

            QJsonDocument doc(textObj);
            QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
            emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_IMG_CHAT_MSG_REQ, jsonData);

            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }
        else if (type == MsgType::FILE_MSG)
        {

        }

        if (pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(0);
            ui->chat_data_list->appendChatItem(pChatItem);
            _unrsp_item_map[uuidString] = pChatItem;
        }
    }

    if (txt_size > 0) {
        qDebug() << "textArray is " << textArray;
        textObj["text_array"] = textArray;
        textObj["fromuid"] = user_info->_uid;
        textObj["thread_id"] = thread_id;
        textObj["touid"] = _chat_data->GetOtherId();

        if (_chat_data->GetOtherId() == 0) {
            QJsonArray toMembers;
            std::vector<int> members = _chat_data->GetGroupMemberUids();
            for (int m_id : members) {
                if (m_id == user_info->_uid) continue;
                toMembers.append(m_id);
            }
            textObj["to_member_uids"] = toMembers;
        }

        QJsonDocument doc(textObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

        txt_size = 0;
        textArray = QJsonArray();
        textObj = QJsonObject();

        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
    }
}

void ChatPage::on_receive_btn_clicked()
{
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Other;
    auto friend_info = UserMgr::GetInstance()->GetFriendById(_chat_data->GetOtherId());
    QString userName = friend_info->_name;
    QString userIcon = friend_info->_icon;

    const QVector<std::shared_ptr<MsgInfo>>& msgList = pTextEdit->getMsgList();
    for (int i = 0; i < msgList.size(); ++i)
    {
        MsgType type = msgList[i]->_msg_type;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
        QWidget *pBubble = nullptr;

        if (type == MsgType::TEXT_MSG) {
            pBubble = new TextBubble(role, msgList[i]->_text_or_url);
        }
        else if (type == MsgType::IMG_MSG) {
            pBubble = new PictureBubble(QPixmap(msgList[i]->_text_or_url), role, msgList[i]->_total_size);
        }
        else if (type == MsgType::FILE_MSG) {

        }

        if (pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(2);
            ui->chat_data_list->appendChatItem(pChatItem);
        }
    }
}

void ChatPage::on_clicked_paused(QString unique_name, TransferType transfer_type)
{
    Q_UNUSED(transfer_type);
    UserMgr::GetInstance()->PauseTransFileByName(unique_name);
}

void ChatPage::on_clicked_resume(QString unique_name, TransferType transfer_type)
{
    UserMgr::GetInstance()->ResumeTransFileByName(unique_name);

    if (transfer_type == TransferType::Upload) {
        FileTcpMgr::GetInstance()->ContinueUploadFile(unique_name);
        return;
    }

    if (transfer_type == TransferType::Download) {
        FileTcpMgr::GetInstance()->ContinueDownloadFile(unique_name);
        return;
    }
}

void ChatPage::slot_clicked_more_label(QString name, ClickLbState state)
{
    Q_UNUSED(name);
    qDebug() << "Current State:" << state;

    hideEmojiMenu();

    // ✅ 如果菜单已显示 → 直接关闭
    if (_more_menu && _more_menu->isVisible()) {
        _more_menu->hide();
        ui->more_lb->ResetNormalState();
        return;
    }

    if (!_more_menu)
    {
        _more_menu = new MoreMenu(this);

        QPushButton* switchBtn = _more_menu->addMenuItem(" 切换历史聊天记录");
        _more_menu->addSeparator();
        QPushButton* switchModel = _more_menu->addMenuItem(" 切换使用的模型");

        connect(switchBtn, &QPushButton::clicked, this, [this]() {
            showAiHistoryWindow();
            ui->more_lb->ResetNormalState();
        });

        connect(switchModel, &QPushButton::clicked, this, [this]() {
            qDebug() << "clear history clicked";
            showAiModelWindow();
            ui->more_lb->ResetNormalState();
        });


        // ✅ 新增：菜单隐藏时同步状态
        connect(_more_menu, &MoreMenu::sig_menu_hidden, this, [this]() {
            ui->more_lb->ResetNormalState();
        });

        connect(_more_menu, &QWidget::destroyed, this, [this]() {
            ui->more_lb->ResetNormalState();
        });
    }

    _more_menu->adjustSize();

    QPoint globalPos = ui->more_lb->mapToGlobal(QPoint(0, 0));

    int x = globalPos.x() + ui->more_lb->width() - _more_menu->width();
    int y = globalPos.y() - _more_menu->height() - 5;

    _more_menu->move(x, y);
    _more_menu->show();
}

void ChatPage::showAiHistoryWindow()
{
    auto* dlg = new AiHistoryDialog(this);

    connect(dlg, &AiHistoryDialog::sig_history_selected,
            this, &ChatPage::slot_ai_history_selected, Qt::UniqueConnection);

    dlg->exec();
}

void ChatPage::showAiModelWindow()
{
    auto* dlg = new AiModelDialog(this);

    connect(dlg, &AiModelDialog::sig_model_selected,
            this, &ChatPage::slot_ai_model_selected, Qt::UniqueConnection);

    dlg->exec();
}

void ChatPage::slot_ai_history_selected(int ai_thread_id)
{
    qDebug() << "[Debug] ChatPage Instance:" << this << "requested ID:" << ai_thread_id;
    emit sig_request_load_ai_history(ai_thread_id);
}

void ChatPage::slot_ai_model_selected(int ai_model_id)
{
    qDebug() << "[Debug] ChatPage Instance:" << this << "requested ID:" << ai_model_id;

    AIMgr::GetInstance()->SetCurAiModel(ai_model_id);

    QString model_name = AIMgr::GetInstance()->GetCurAiModelName();
    ui->title_lb->setText("AI " + model_name);
}

void ChatPage::clearItems()
{
    if (_current_thread_id >= 0) {
        ClearThreadCache(_current_thread_id);
    }
    ClearCurrentUiOnly();
}

void ChatPage::AppendOtherMsg(std::shared_ptr<ChatDataBase> msg)
{
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role;

    if (msg->GetSendUid() == self_info->_uid) {
        role = ChatRole::Self;
        ChatItemBase* pChatItem = new ChatItemBase(role);

        pChatItem->setUserName(self_info->_name);
        SetSelfIcon(pChatItem, self_info->_icon);
        QWidget* pBubble = nullptr;

        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        else if (msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = dynamic_pointer_cast<ImgChatData>(msg);
            auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;

            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }

        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);
        _base_item_map[msg->GetMsgId()] = pChatItem;
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());
        if (friend_info == nullptr) {
            return;
        }
        pChatItem->setUserName(friend_info->_name);

        QRegularExpression regex("^:/res/head_(\\d+)\\.jpg$");
        QRegularExpressionMatch match = regex.match(friend_info->_icon);
        if (match.hasMatch()) {
            pChatItem->setUserIcon(QPixmap(friend_info->_icon));
        }
        else {
            QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            auto uid = UserMgr::GetInstance()->GetUid();
            QDir avatarsDir(storageDir + "/user/" + QString::number(uid) + "/avatars");
            auto file_name = QFileInfo(self_info->_icon).fileName();

            if (avatarsDir.exists()) {
                QString avatarPath = avatarsDir.filePath(file_name);
                QPixmap pixmap(avatarPath);
                if (!pixmap.isNull()) {
                    pChatItem->setUserIcon(pixmap);
                }
                else {
                    qWarning() << "无法加载上传的头像：" << avatarPath;
                    auto icon_label = pChatItem->getIconLabel();
                    LoadHeadIcon(avatarPath, icon_label, file_name, "self_icon");
                }
            }
            else {
                qWarning() << "头像存储目录不存在：" << avatarsDir.path();
                avatarsDir.mkpath(".");
                auto icon_label = pChatItem->getIconLabel();
                QString avatarPath = avatarsDir.filePath(file_name);
                LoadHeadIcon(avatarPath, icon_label, file_name, "self_icon");
            }
        }

        QWidget* pBubble = nullptr;
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        else if (msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = dynamic_pointer_cast<ImgChatData>(msg);
            auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;

            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }

        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);
        _base_item_map[msg->GetMsgId()] = pChatItem;
    }
}

void ChatPage::LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type)
{
    UserMgr::GetInstance()->AddLabelToReset(avatarPath, icon_label);

    QPixmap pixmap(":/res/head_1.jpg");
    QPixmap scaledPixmap = pixmap.scaled(icon_label->size(),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    icon_label->setPixmap(scaledPixmap);
    icon_label->setScaledContents(true);

    bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
    if (is_loading) {
        qWarning() << "正在下载: " << file_name;
    }
    else {
        auto download_info = std::make_shared<DownloadInfo>();
        download_info->_name = file_name;
        download_info->_current_size = 0;
        download_info->_seq = 1;
        download_info->_total_size = 0;
        download_info->_client_path = avatarPath;

        UserMgr::GetInstance()->AddDownloadFile(file_name, download_info);
        FileTcpMgr::GetInstance()->SendDownloadInfo(download_info, req_type);
    }
}

void ChatPage::DownloadFileFinished(std::shared_ptr<MsgInfo> msg_info, QString file_path)
{
    auto iter = _base_item_map.find(msg_info->_msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        auto bubble = iter.value()->getBubble();
        PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        pic_bubble->setDownloadFinish(msg_info, file_path);

        auto chat_data_base = _chat_data->GetChatDataBase(msg_info->_msg_id);
        if (chat_data_base == nullptr) {
            return;
        }

        auto img_data = dynamic_pointer_cast<ImgChatData>(chat_data_base);
        img_data->_msg_info->_preview_pix = QPixmap(file_path);
        img_data->_msg_info->_transfer_state = TransferState::Completed;
        img_data->_msg_info->_current_size = img_data->_msg_info->_total_size;
    }
}

void ChatPage::showEmojiMenu()
{
    if (!_emoji_menu) {
        _emoji_menu = new EmojiMenu(this);
        connect(_emoji_menu, &EmojiMenu::sig_emoji_selected,
                this, &ChatPage::slot_insert_emoji);
    }

    _emoji_menu->adjustSize();

    QPoint anchor = ui->emo_lb->mapToGlobal(QPoint(0, 0));

    int x = anchor.x();
    int y = anchor.y() - _emoji_menu->height();

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    _emoji_menu->move(x, y);
    _emoji_menu->show();
    _emoji_menu->raise();
    _emoji_menu->setFocus();
}

void ChatPage::hideEmojiMenu()
{
    if (_emoji_menu) {
        _emoji_menu->hide();
    }
}

void ChatPage::slot_clicked_emoji_label(QString name, ClickLbState state)
{
    Q_UNUSED(name);
    Q_UNUSED(state);

    if (_emoji_menu && _emoji_menu->isVisible()) {
        hideEmojiMenu();
        ui->emo_lb->ResetNormalState();
        return;
    }

    showEmojiMenu();
}

void ChatPage::slot_insert_emoji(const QString &token)
{
    if (!ui || !ui->chatEdit) {
        return;
    }

    ui->chatEdit->insertEmojiToken(token);
    ui->chatEdit->setFocus();
    ui->emo_lb->ResetNormalState();
}
