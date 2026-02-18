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

ChatPage::ChatPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    //设置按钮样式
    ui->receive_btn->SetState("normal","hover","press");
    ui->send_btn->SetState("normal","hover","press");

    //设置图标样式
    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");
    ui->more_lb->SetState("normal","hover","press","normal","hover","press");

    connect(ui->more_lb, &ClickedLabel::clicked, this, &ChatPage::slot_clicked_more_label);
}

ChatPage::~ChatPage()
{
    delete ui;
}

//在页面具体设置数据信息
void ChatPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;
    auto other_id = _chat_data->GetOtherId();


    if(other_id == -1) {
        //说明是ai
        ui->title_lb->setText("AI");

        ui->chat_data_list->removeAllItem();
        _unrsp_item_map.clear();
        _base_item_map.clear();

        for (auto& msg : chat_data->GetMsgMapRef()) {
            auto text_msg = std::dynamic_pointer_cast<TextChatData>(msg);

            if (text_msg) {
                AppendAiChatMsg(text_msg);
            } else {
                // 可以记录日志，或者忽略非 TextChatData 的消息
                qDebug() << "跳过非文本消息类型";
            }
        }

        for (auto& msg : chat_data->GetMsgUnRspRef()) {
            auto text_msg = std::dynamic_pointer_cast<TextChatData>(msg);

            if (text_msg) {
                AppendAiChatMsg(text_msg);
            } else {
                // 可以记录日志，或者忽略非 TextChatData 的消息
                qDebug() << "跳过非文本消息类型";
            }
        }
        return;
    }

    if(other_id == 0) {
        //说明是群聊
        ui->title_lb->setText(_chat_data->GetGroupName());

        ui->chat_data_list->removeAllItem();
        _unrsp_item_map.clear();
        _base_item_map.clear();

        // 1. 加载历史消息（和私聊完全一样）
        for (auto& msg : chat_data->GetMsgMapRef()) {
            AppendChatMsg(msg);
        }

        for (auto& msg : chat_data->GetMsgUnRspRef()) {
            AppendChatMsg(msg);
        }
        return;
    }

    //私聊
    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);
    if (friend_info == nullptr) {
        return;
    }
    ui->title_lb->setText(friend_info->_name);
    ui->chat_data_list->removeAllItem();
    _unrsp_item_map.clear();
    _base_item_map.clear();
    for(auto & msg : chat_data->GetMsgMapRef()){
        AppendChatMsg(msg);
    }

    for (auto& msg : chat_data->GetMsgUnRspRef()) {
        AppendChatMsg(msg);
    }
}

void ChatPage::AppendChatMsg(std::shared_ptr<ChatDataBase> msg)
{
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role;
    if (msg->GetSendUid() == self_info->_uid) {
        role = ChatRole::Self;
        // 聊天基类, 名字与头像的设置
        ChatItemBase* pChatItem = new ChatItemBase(role);
        
        pChatItem->setUserName(self_info->_name);
        SetSelfIcon(pChatItem, self_info->_icon);
        QWidget* pBubble = nullptr;
        // 如果是文字,则有一个文字专属的bubble
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
     
        pChatItem->setWidget(pBubble);
        // 设置当前状态,消息当中有
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        //消息列表中放入这条消息
        ui->chat_data_list->appendChatItem(pChatItem);



        //而且是没有收到服务器确认的, 这个可能需要改一改,下面也有一个一样的
        _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());
        if (friend_info == nullptr) {
            return;
        }
        pChatItem->setUserName(friend_info->_name);
        
        // 使用正则表达式检查是否是默认头像
        QRegularExpression regex("^:/res/head_(\\d+)\\.jpg$");
        QRegularExpressionMatch match = regex.match(friend_info->_icon);
        if (match.hasMatch()) {
            pChatItem->setUserIcon(QPixmap(friend_info->_icon));
        }
        else {
            // 如果是用户上传的头像，获取存储目录
            QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir avatarsDir(storageDir + "/avatars");
            auto file_name = QFileInfo(self_info->_icon).fileName();
            // 确保目录存在
            if (avatarsDir.exists()) {
                QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
                QPixmap pixmap(avatarPath); // 加载上传的头像图片
                if (!pixmap.isNull()) {
                    pChatItem->setUserIcon(pixmap);
                }
                else {
                    qWarning() << "无法加载上传的头像：" << avatarPath;
                    auto icon_label = pChatItem->getIconLabel();
                    UserMgr::GetInstance()->AddLabelToReset(avatarPath, icon_label);
                    //先加载默认的
                    QPixmap pixmap(":/res/head_1.jpg");
                    QPixmap scaledPixmap = pixmap.scaled(icon_label->size(),
                        Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    icon_label->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    icon_label->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

                    //判断是否正在下载
                    bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                    if (is_loading) {
                        qWarning() << "正在下载: " << file_name;
                    }
                    else {
                        //发送请求获取资源
                        auto download_info = std::make_shared<DownloadInfo>();
                        download_info->_name = file_name;
                        download_info->_current_size = 0;
                        download_info->_seq = 1;
                        download_info->_total_size = 0;
                        download_info->_client_path = avatarPath;
                        //添加文件到管理者
                        UserMgr::GetInstance()->AddDownloadFile(file_name, download_info);
                        //发送消息
                        FileTcpMgr::GetInstance()->SendDownloadInfo(download_info);
                    }
                }
            }
            else {
                qWarning() << "头像存储目录不存在：" << avatarsDir.path();
            }
        }

        QWidget* pBubble = nullptr;
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);
        _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
    }

}



void ChatPage::UpdateChatStatus(std::shared_ptr<ChatDataBase> msg)
{
    auto iter = _unrsp_item_map.find(msg->GetUniqueId());
    //没找到则直接返回
    if (iter == _unrsp_item_map.end()) {
        iter.value()->setStatus(msg->GetStatus());
        _base_item_map[msg->GetMsgId()] = iter.value();
        return;
    }

    iter.value()->setStatus(msg->GetStatus());
    _base_item_map[msg->GetMsgId()] = iter.value();
    _unrsp_item_map.erase(iter);
}

void ChatPage::UpdateImgChatStatus(std::shared_ptr<ImgChatData> msg) {
    auto iter = _unrsp_item_map.find(msg->GetUniqueId());
    //没找到则直接返回
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

void ChatPage::UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info) {
    auto iter = _base_item_map.find(msg_info->_msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        auto bubble = iter.value()->getBubble();
        PictureBubble*  pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        pic_bubble->setProgress(msg_info->_rsp_size);
    }
    
}

void ChatPage::SetSelfIcon(ChatItemBase* pChatItem, QString icon)
{
    // 使用正则表达式检查是否是默认头像
    QRegularExpression regex("^:/res/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(icon);
    if (match.hasMatch()) {
        pChatItem->setUserIcon(QPixmap(icon));
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir avatarsDir(storageDir + "/avatars");
        auto file_name = QFileInfo(icon).fileName();
        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                pChatItem->setUserIcon(pixmap);
            }
            else {
                qWarning() << "无法加载上传的头像：" << avatarPath;
                auto icon_label = pChatItem->getIconLabel();
                UserMgr::GetInstance()->AddLabelToReset(avatarPath, icon_label);
                //先加载默认的
                QPixmap pixmap(":/res/head_1.jpg");
                QPixmap scaledPixmap = pixmap.scaled(icon_label->size(),
                    Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                icon_label->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                icon_label->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

                //判断是否正在下载
                bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                if (is_loading) {
                    qWarning() << "正在下载: " << file_name;
                }
                else {
                    //发送请求获取资源
                    auto download_info = std::make_shared<DownloadInfo>();
                    download_info->_name = file_name;
                    download_info->_current_size = 0;
                    download_info->_seq = 1;
                    download_info->_total_size = 0;
                    download_info->_client_path = avatarPath;
                    //添加文件到管理者
                    UserMgr::GetInstance()->AddDownloadFile(file_name, download_info);
                    //发送消息
                    FileTcpMgr::GetInstance()->SendDownloadInfo(download_info);
                }
            }
        }
        else {
            qWarning() << "头像存储目录不存在：" << avatarsDir.path();
        }
    }
}

void ChatPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}


void ChatPage::AppendAiChatMsg(std::shared_ptr<TextChatData> msg)
{
    ChatRole role;
    if(msg->GetSendUid() == AI_UID){
        role = ChatRole::Other;
    }else{
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
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        icon_label->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        icon_label->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }

    QWidget* pBubble = new TextBubble(role, msg->GetContent());
    pChatItem->setWidget(pBubble);
    pChatItem->setStatus(msg->GetStatus()); // AI

    ui->chat_data_list->appendChatItem(pChatItem);

    _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
}


void ChatPage::UpdateAiChatStatus(QString unique_id, int msg_id)
{
    auto iter = _unrsp_item_map.find(unique_id);
    //没找到则直接返回
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

    // 目前 AI 先只支持文本（这是正确的）
    auto msg = msgList[0];
    if (msg->_msg_type != MsgType::TEXT_MSG) {
        qDebug() << "AI only supports text currently";
        return;
    }

    QString content = msg->_text_or_url;
    if (content.isEmpty()) return;




    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(AI_THREAD);

    QUuid uuid = QUuid::createUuid();
    //转为字符串
    QString uuidString = uuid.toString();

    auto txt_msg = std::make_shared<TextChatData>(uuidString, AI_THREAD, ChatFormType::AI,
                                                  ChatMsgType::TEXT, content, user_info->_uid, 0);

    AppendAiChatMsg(txt_msg);
    chat_data->AppendUnRspMsg(uuidString, txt_msg);



    // ---------- 1. UI 先插入“自己发的消息” ----------
    // ChatItemBase* pChatItem = new ChatItemBase(ChatRole::Self);
    // pChatItem->setUserName(user_info->_name);
    // SetSelfIcon(pChatItem, user_info->_icon);

    // auto bubble = new TextBubble(ChatRole::Self, content);
    // pChatItem->setWidget(bubble);
    // pChatItem->setStatus(0);

    // ui->chat_data_list->appendChatItem(pChatItem);

    // ---------- 2. 组织 HTTP 请求 ----------
    QJsonObject req;
    req["uid"] = user_info->_uid;
    //req["token"] = UserMgr::GetInstance()->GetToken();
    req["content"] = content;
    req["model"] = AIMgr::GetInstance()->GetCurAiModelName(); // 或客户端当前选中的模型
    req["unique_id"] = uuidString;
    // ai_thread_id：-1 表示新会话
    req["ai_thread_id"] = AIMgr::GetInstance()->GetCurAiThread(); // -1 或真实 id

    // ---------- 3. 发送 HTTP ----------
    QUrl url("http://127.0.0.1:8070/ai/chat");
    HttpMgr::GetInstance()->PostHttpReq(
        url,
        req,
        ReqId::ID_AI_CHAT_REQ,
        Modules::AIMOD
        );

    // 清空输入框
    pTextEdit->clear();
}




void ChatPage::on_send_btn_clicked() {
    if (_chat_data == nullptr) {
        qDebug() << "thread chat info is empty";
        return;
    }

    // ⭐ 新增：AI 会话分流
    if (_chat_data->GetOtherId() == -1) {
        send_msg_to_ai();
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Self;
    QString userName = user_info->_name;
    QString userIcon = user_info->_icon;

    const QVector<std::shared_ptr<MsgInfo>>& msgList = pTextEdit->getMsgList();
    QJsonObject textObj;
    QJsonArray textArray;
    int txt_size = 0;
    auto thread_id = _chat_data->GetThreadId();
    for (int i = 0; i < msgList.size(); ++i)
    {
        //消息内容长度不合规就跳过
        if (msgList[i]->_text_or_url.length() > 1024) {
            continue;
        }

        MsgType type = msgList[i]->_msg_type;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        SetSelfIcon(pChatItem, user_info->_icon);
        QWidget* pBubble = nullptr;
        //生成唯一id
        QUuid uuid = QUuid::createUuid();
        //转为字符串
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
                if(other_id == 0){
                    // 群聊/多人群组
                    QJsonArray toMembers;
                    std::vector<int> members = _chat_data->GetGroupMemberUids(); // 获取成员列表

                    for (int m_id : members) {
                        // 注意：通常不需要发给自己，可以在这里过滤掉自己, 我好像在拿到群聊的时候就已经过滤了?
                        if (m_id == user_info->_uid) continue;
                        toMembers.append(m_id);
                    }
                    textObj["to_member_uids"] = toMembers; // 增加多人 ID 数组结构
                }
                // -------------------

                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                //发送并清空之前累计的文本列表
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();
                //发送tcp请求给chat server
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            //将bubble和uid绑定，以后可以等网络返回消息后设置是否送达
            //_bubble_map[uuidString] = pBubble;
            txt_size += msgList[i]->_text_or_url.length();
            QJsonObject obj;
            QByteArray utf8Message = msgList[i]->_text_or_url.toUtf8();
            auto content = QString::fromUtf8(utf8Message);
            obj["content"] = content;
            obj["unique_id"] = uuidString;
            textArray.append(obj);
            //注意，这里私聊和群聊的处理

            ChatFormType chat_type;
            if(other_id == 0){
                chat_type = ChatFormType::GROUP;
            }else{
                chat_type = ChatFormType::PRIVATE;
            }

            auto txt_msg = std::make_shared<TextChatData>(uuidString, thread_id, chat_type,
                ChatMsgType::TEXT, content, user_info->_uid, 0);
            //将未回复的消息加入到未回复列表中，以便后续处理
            _chat_data->AppendUnRspMsg(uuidString, txt_msg);
        }
        else if (type == MsgType::IMG_MSG)
        {
            int other_id = _chat_data->GetOtherId();
            //将之前缓存的文本发送过去
            if (txt_size) {
                textObj["fromuid"] = user_info->_uid;
                textObj["thread_id"] = thread_id;
                textObj["text_array"] = textArray;


                textObj["touid"] = other_id;
                if(other_id == 0){
                    // 群聊/多人群组
                    QJsonArray toMembers;
                    std::vector<int> members = _chat_data->GetGroupMemberUids(); // 获取成员列表

                    for (int m_id : members) {
                        // 注意：通常不需要发给自己，可以在这里过滤掉自己, 我好像在拿到群聊的时候就已经过滤了?
                        if (m_id == user_info->_uid) continue;
                        toMembers.append(m_id);
                    }
                    textObj["to_member_uids"] = toMembers; // 增加多人 ID 数组结构
                }

                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                //发送并清空之前累计的文本列表
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();
                //发送tcp请求给chat server
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            pBubble = new PictureBubble(QPixmap(msgList[i]->_text_or_url), role, msgList[i]->_total_size);
            //需要组织成文件发送，具体参考头像上传

            ChatFormType chat_type;
            if(other_id == 0){
                chat_type = ChatFormType::GROUP;
            }else{
                chat_type = ChatFormType::PRIVATE;
            }

            auto img_msg = std::make_shared<ImgChatData>(msgList[i],uuidString, thread_id, chat_type,
                ChatMsgType::TEXT, user_info->_uid, 0);
            //将未回复的消息加入到未回复列表中，以便后续处理
            _chat_data->AppendUnRspMsg(uuidString, img_msg);
            textObj["fromuid"] = user_info->_uid;
            textObj["thread_id"] = thread_id;
            textObj["md5"] = msgList[i]->_md5;
            textObj["name"] = msgList[i]->_unique_name;
            textObj["token"] = UserMgr::GetInstance()->GetToken();
            textObj["unique_id"] = uuidString;
            textObj["text_or_url"] = msgList[i]->_text_or_url;


            textObj["touid"] = other_id;
            if(other_id == 0){
                // 群聊/多人群组
                QJsonArray toMembers;
                std::vector<int> members = _chat_data->GetGroupMemberUids(); // 获取成员列表

                for (int m_id : members) {
                    // 注意：通常不需要发给自己，可以在这里过滤掉自己, 我好像在拿到群聊的时候就已经过滤了?
                    if (m_id == user_info->_uid) continue;
                    toMembers.append(m_id);
                }
                textObj["to_member_uids"] = toMembers; // 增加多人 ID 数组结构
            }

            //文件信息加入管理
            UserMgr::GetInstance()->AddTransFile(msgList[i]->_unique_name, msgList[i]);
            QJsonDocument doc(textObj);
            QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
            //发送tcp请求给chat server
            emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_IMG_CHAT_MSG_REQ, jsonData);


            //链接暂停信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                this, &ChatPage::on_clicked_paused);
            //链接恢复信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                this, &ChatPage::on_clicked_resume);

        }
        else if (type == MsgType::FILE_MSG)
        {

        }
        //发送消息
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
        //发送给服务器
        textObj["text_array"] = textArray;
        textObj["fromuid"] = user_info->_uid;
        textObj["thread_id"] = thread_id;

        textObj["touid"] = _chat_data->GetOtherId();
        if(_chat_data->GetOtherId() == 0){
            // 群聊/多人群组
            QJsonArray toMembers;
            std::vector<int> members = _chat_data->GetGroupMemberUids(); // 获取成员列表

            for (int m_id : members) {
                // 注意：通常不需要发给自己，可以在这里过滤掉自己, 我好像在拿到群聊的时候就已经过滤了?
                if (m_id == user_info->_uid) continue;
                toMembers.append(m_id);
            }
            textObj["to_member_uids"] = toMembers; // 增加多人 ID 数组结构
        }

        QJsonDocument doc(textObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        //发送并清空之前累计的文本列表
        txt_size = 0;
        textArray = QJsonArray();
        textObj = QJsonObject();
        //发送tcp请求给chat server
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
    for(int i=0; i<msgList.size(); ++i)
    {
        MsgType type = msgList[i]->_msg_type;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
        QWidget *pBubble = nullptr;
        if(type == MsgType::TEXT_MSG)
        {
            pBubble = new TextBubble(role, msgList[i]->_text_or_url);
        }
        else if(type == MsgType::IMG_MSG)
        {
            pBubble = new PictureBubble(QPixmap(msgList[i]->_text_or_url) , role, msgList[i]->_total_size);
        }
        else if(type == MsgType::FILE_MSG)
        {

        }
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(2);
            ui->chat_data_list->appendChatItem(pChatItem);
        }
    }
}

void ChatPage::on_clicked_paused(QString unique_name, TransferType transfer_type)
{
    UserMgr::GetInstance()->PauseTransFileByName(unique_name);
}

void ChatPage::on_clicked_resume(QString unique_name, TransferType transfer_type)
{
    UserMgr::GetInstance()->ResumeTransFileByName(unique_name);
    //继续发送或者下载
    if (transfer_type == TransferType::Upload) {
        FileTcpMgr::GetInstance()->ContinueUploadFile(unique_name);
        return;
    }

    if (transfer_type == TransferType::Download) {
        return;
    }
}

void ChatPage::slot_clicked_more_label(QString name, ClickLbState state)
{
    qDebug() << "Current State:" << state;

    if (state != ClickLbState::Selected)
        return;

    if (!_more_menu)
    {
        _more_menu = new MoreMenu(this);

        // 添加菜单项
        QPushButton* switchBtn =
            _more_menu->addMenuItem(" 切换历史聊天记录");

        _more_menu->addSeparator();

        QPushButton* switchModel =
            _more_menu->addMenuItem(" 切换使用的模型");

        // 连接信号
        connect(switchBtn, &QPushButton::clicked, this, [this]() {
            showAiHistoryWindow();
            ui->more_lb->ResetNormalState();
        });

        connect(switchModel, &QPushButton::clicked, this, [this]() {
            // 这里写清空逻辑
            qDebug() << "clear history clicked";
            showAiModelWindow();
            ui->more_lb->ResetNormalState();
        });

        // 当菜单隐藏时，重置按钮状态
        connect(_more_menu, &QWidget::destroyed, this, [this]() {
            ui->more_lb->ResetNormalState();
        });
    }

    // 每次显示前重新计算尺寸
    _more_menu->adjustSize();

    QPoint globalPos =
        ui->more_lb->mapToGlobal(QPoint(0, 0));

    int x = globalPos.x()
            + ui->more_lb->width()
            - _more_menu->width();

    int y = globalPos.y()
            - _more_menu->height()
            - 5;

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


// 往上级传递
void ChatPage::slot_ai_history_selected(int ai_thread_id)
{
    qDebug() << "[Debug] ChatPage Instance:" << this << "requested ID:" << ai_thread_id;
    emit sig_request_load_ai_history(ai_thread_id);
}

// 往上级传递
void ChatPage::slot_ai_model_selected(int ai_model_id)
{
    qDebug() << "[Debug] ChatPage Instance:" << this << "requested ID:" << ai_model_id;


    AIMgr::GetInstance()->SetCurAiModel(ai_model_id);

    QString model_name = AIMgr::GetInstance()->GetCurAiModelName();
    // 这里直接换啊
    ui->title_lb->setText("AI " + model_name);

    // emit sig_request_change_ai_model(ai_model_id);
}



void ChatPage::clearItems()
{
    ui->chat_data_list->removeAllItem();
    _unrsp_item_map.clear();
    _base_item_map.clear();
}
