#ifndef USERDATA_H
#define USERDATA_H
#include <QString>
#include <memory>
#include <QJsonArray>
#include <vector>
#include <QJsonObject>
#include <QMap>
#include "global.h"

class SearchInfo {
public:
    SearchInfo(int uid, QString name, QString nick, QString desc, int sex, QString icon);
    SearchInfo() = default;
	int _uid;
	QString _name;
	QString _nick;
	QString _desc;
	int _sex;
    QString _icon;
};

Q_DECLARE_METATYPE(SearchInfo)
Q_DECLARE_METATYPE(std::shared_ptr<SearchInfo>)

class AddFriendApply {
public:
    AddFriendApply(int from_uid, QString name, QString desc,
                   QString icon, QString nick, int sex);
    AddFriendApply() = default;
	int _from_uid;
	QString _name;
	QString _desc;
    QString _icon;
    QString _nick;
    int     _sex;
};

Q_DECLARE_METATYPE(std::shared_ptr<AddFriendApply>)

struct ApplyInfo {
    ApplyInfo() = default;
    ApplyInfo(int uid, QString name, QString desc,
        QString icon, QString nick, int sex, int status)
        :_uid(uid),_name(name),_desc(desc),
        _icon(icon),_nick(nick),_sex(sex),_status(status){}

    ApplyInfo(std::shared_ptr<AddFriendApply> addinfo)
        :_uid(addinfo->_from_uid),_name(addinfo->_name),
          _desc(addinfo->_desc),_icon(addinfo->_icon),
          _nick(addinfo->_nick),_sex(addinfo->_sex),
          _status(0)
    {}
    void SetIcon(QString head){
        _icon = head;
    }
    int _uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int _sex;
    int _status;
};

class TextChatData;
struct AuthInfo {
    AuthInfo(int uid, QString name,
             QString nick, QString icon, int sex):
        _uid(uid), _name(name), _nick(nick), _icon(icon),
        _sex(sex), _thread_id(0){}
    AuthInfo() = default;
    void SetChatDatas(std::vector<std::shared_ptr<TextChatData>> _chat_datas);
    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _thread_id;
    std::vector<std::shared_ptr<TextChatData>> _chat_datas;
};

Q_DECLARE_METATYPE(std::shared_ptr<AuthInfo>)

struct AuthRsp {
    AuthRsp() = default;
    AuthRsp(int peer_uid, QString peer_name,
            QString peer_nick, QString peer_icon, int peer_sex)
        :_uid(peer_uid),_name(peer_name),_nick(peer_nick),
          _icon(peer_icon),_sex(peer_sex),_thread_id(0)
    {
    
    }


    void SetChatDatas(std::vector<std::shared_ptr<TextChatData>> _chat_datas);
    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _thread_id;
    std::vector<std::shared_ptr<TextChatData>> _chat_datas;
};

Q_DECLARE_METATYPE(std::shared_ptr<AuthRsp>)

struct UserInfo {
    UserInfo(int uid, QString name, QString nick, QString icon, int sex, QString last_msg = "", QString desc=""):
        _uid(uid),_name(name),_nick(nick),_icon(icon),_sex(sex),_desc(desc){}

    UserInfo(std::shared_ptr<AuthInfo> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick),
        _icon(auth->_icon),_sex(auth->_sex),_desc(""){}

    UserInfo(int uid, QString name, QString icon):
    _uid(uid), _name(name), _icon(icon),_nick(_name),
    _sex(0),_desc(""){

    }

    UserInfo(std::shared_ptr<AuthRsp> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick),
        _icon(auth->_icon),_sex(auth->_sex),_desc(""){}

    UserInfo(std::shared_ptr<SearchInfo> search_info):
        _uid(search_info->_uid),_name(search_info->_name),_nick(search_info->_nick),
    _icon(search_info->_icon),_sex(search_info->_sex), _desc(search_info->_desc){

    }

    UserInfo() = default;

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    QString _desc;
};

Q_DECLARE_METATYPE(std::shared_ptr<UserInfo>)

class ChatDataBase {
public:
    ChatDataBase() = default;
    ChatDataBase(int msg_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                 QString content,int _send_uid, int status, QString chat_time );
    ChatDataBase(QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                 QString content, int send_uid, int status, QString chat_time);
    ChatDataBase(int msg_id, QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                 QString content, int send_uid, int status, QString chat_time);
    int GetMsgId() { return _msg_id; }
    int GetThreadId() { return _thread_id; }
    ChatFormType GetFormType() { return _form_type; }
    ChatMsgType GetMsgType() { return _msg_type; }
    QString GetContent() { return _content; }
    int GetSendUid() { return _send_uid; }
    QString GetMsgContent(){return _content;}
    void SetUniqueId(int unique_id);
    QString GetUniqueId();
    int GetStatus() { return _status; }
    void SetMsgId(int msg_id) { _msg_id = msg_id; }
    void SetStatus(int status) { _status = status; }
    virtual void dump(QDebug dbg) const {
        dbg.nospace() << "ChatDataBase("
                      << "ID:" << _msg_id
                      << ", UniqueID:" << _unique_id
                      << ", ThreadID:" << _thread_id
                      << ", Type:" << (int)_msg_type
                      << ", Content:" << _content
                      << ", Status:" << _status << ")";
    }

    // 重载运算符
    friend QDebug operator<<(QDebug dbg, const ChatDataBase &data) {
        data.dump(dbg);
        return dbg;
    }

    // 为了支持 shared_ptr 的直接打印，也可以重载指针版本
    friend QDebug operator<<(QDebug dbg, const std::shared_ptr<ChatDataBase> &data) {
        if (!data) return dbg << "ChatDataBase(nullptr)";
        data->dump(dbg);
        return dbg;
    }

    virtual ~ChatDataBase() {}  // 添加虚析构函数
protected:
    //客户端本地唯一标识
    QString _unique_id;
    //消息id
    int _msg_id;
    //会话id
    int _thread_id;
    //群聊还是私聊
    ChatFormType _form_type;
    //文本信息为0，图片为1，文件为2
    ChatMsgType _msg_type;
    QString _content;
    //发送者id
    int _send_uid;
    //状态
    int _status;
    //聊天时间
    QString _chat_time;
};

Q_DECLARE_METATYPE(std::vector<std::shared_ptr<ChatDataBase>>)

class TextChatData : public ChatDataBase {
public:

    TextChatData(int msg_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,  QString content,
        int send_uid, int status, QString chat_time="") :
        ChatDataBase(msg_id, thread_id, form_type, msg_type, content, send_uid, status, chat_time)
    {
        
    }

    TextChatData(QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type, QString content,
        int send_uid, int status, QString chat_time="") :
        ChatDataBase(unique_id, thread_id, form_type, msg_type, content, send_uid, status, chat_time)
    {
        
    }

    TextChatData(int msg_id, QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type, QString content,
        int send_uid, int status, QString chat_time = "") :
        ChatDataBase(msg_id, unique_id, thread_id, form_type, msg_type, content, send_uid, status, chat_time)
    {

    }

    TextChatData() = default;

    ~TextChatData() override{}
};

Q_DECLARE_METATYPE(std::vector<std::shared_ptr<TextChatData>>)


class ImgChatData : public ChatDataBase {
public:
    ImgChatData(std::shared_ptr<MsgInfo> msg_info, QString unique_id,
                int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                int send_uid, int status, QString chat_time = ""):
        ChatDataBase(unique_id,thread_id, form_type, msg_type, msg_info->_text_or_url,
                     send_uid, status, chat_time), _msg_info(msg_info){
        _msg_id = _msg_info->_msg_id;
    }

    void dump(QDebug dbg) const override {
        dbg.nospace() << "ImgChatData("
                      << "ID:" << _msg_id
                      << ", Content:" << _content
                      << ", HasMsgInfo:" << (_msg_info != nullptr) << ")";
    }
    ~ImgChatData() override {}

    std::shared_ptr<MsgInfo> _msg_info;
};

Q_DECLARE_METATYPE(std::shared_ptr<ImgChatData>)


struct GroupInfo {
    int _role = 0;
    QString _name;              // 用户原始昵称
    QString _mute_until;
    QString _group_nickname;    // 群昵称/群名片

    GroupInfo() = default;

    GroupInfo(int role, const QString& name,
              const QString& mute_until = "",
              const QString& group_nickname = "")
        : _role(role),
        _name(name),
        _mute_until(mute_until),
        _group_nickname(group_nickname) {}
};
Q_DECLARE_METATYPE(std::shared_ptr<GroupInfo>)

struct GroupMemberBrief {
    int _uid = 0;
    QString _name;
    int _role = 0;

    GroupMemberBrief() = default;
    GroupMemberBrief(int uid, const QString& name, int role)
        : _uid(uid), _name(name), _role(role) {}
};

struct GroupChatInitData {
    int _thread_id = 0;
    QString _group_name;
    int _member_count = 0;
    std::vector<int> _member_ids;
    QMap<int, std::shared_ptr<GroupInfo>> _member_infos;
};

Q_DECLARE_METATYPE(GroupChatInitData)
Q_DECLARE_METATYPE(GroupMemberBrief)

Q_DECLARE_METATYPE(std::shared_ptr<GroupMemberBrief>)
Q_DECLARE_METATYPE(std::shared_ptr<GroupChatInitData>)


//聊天线程信息
struct ChatThreadInfo {
    int _thread_id;
    QString _type;     // "private" or "group"
    int _user1_id;    // 私聊时对应 private_chat.user1_id；群聊时设为 0
    int _user2_id;    // 私聊时对应 private_chat.user2_id；群聊时设为 0

    std::vector<int> _member_ids; // 群聊成员列表，私聊时为空
    QMap<int, std::shared_ptr<GroupInfo>> _meber_infos;
    ChatThreadInfo() = default;
};



Q_DECLARE_METATYPE(std::vector<std::shared_ptr<ChatThreadInfo>>)


//客户端本地存储的聊天线程数据结构
class ChatThreadData {
public:
    ChatThreadData() = default;

    ChatThreadData(int other_id, int thread_id, int last_msg_id)
        : _other_id(other_id), _thread_id(thread_id), _last_msg_id(last_msg_id) {
        _thread_type = ChatFormType::PRIVATE;
    }

    ChatThreadData(std::vector<int> member_ids,
                   int thread_id,
                   int last_msg_id,
                   const QString& group_name,
                   const QMap<int, std::shared_ptr<GroupInfo>>& group_members_info)
        : _group_members(member_ids),
        _thread_id(thread_id),
        _last_msg_id(last_msg_id),
        _other_id(0),
        _group_members_info(group_members_info),
        _group_name(group_name),
        _thread_type(ChatFormType::GROUP)
    {
        // self role 不应该是0才对 todo
        self_role = 0;
    }

    ChatThreadData(int thread_id, int last_msg_id)
        : _last_msg_id(last_msg_id), _thread_id(thread_id) {
        _other_id = -1;
        _thread_type = ChatFormType::AI;
    }

    void AddMsg(std::shared_ptr<ChatDataBase> msg);
    void MoveMsg(std::shared_ptr<ChatDataBase> msg);
    void UpdateProgress(std::shared_ptr<MsgInfo> msg);
    void UpdateAiMsg(QString unique_id, int msg_id);


    QMap<int, std::shared_ptr<ChatDataBase>> GetMsgMap();

    QMap<int, std::shared_ptr<ChatDataBase>>&  GetMsgMapRef();
    void AppendMsg(int msg_id, std::shared_ptr<ChatDataBase> base_msg);
    QString GetLastMsg();

    QMap<QString, std::shared_ptr<ChatDataBase>>& GetMsgUnRspRef();
    void AppendUnRspMsg(QString unique_id, std::shared_ptr<ChatDataBase> base_msg);
    std::shared_ptr<ChatDataBase> GetChatDataBase(int msg_id);


    void ClearChatMsg();
    void UpdateImgChatStatusByMsgId(int msg_id);

    QString GetGroupName() { return _group_name; }
    std::vector<int> GetGroupMemberUids() { return _group_members; }
    int GetThreadId() { return _thread_id; }
    int GetOtherId() { return _other_id; }
    void SetOtherId(int other_id) { _other_id = other_id; }
    int GetLastMsgId() { return _last_msg_id; }
    void SetLastMsgId(int msg_id) { _last_msg_id = msg_id; }

    QMap<int, std::shared_ptr<GroupInfo>>& GetGroupMemberInfoRef() {
        return _group_members_info;
    }

    int GetSelfRole() const { return self_role; }
    int GetSelfUid() const { return _self_uid; }

private:
    int _other_id = 0;
    int _last_msg_id = 0;
    int _thread_id = 0;
    QString _last_msg;

    std::vector<int> _group_members;
    QMap<int, std::shared_ptr<GroupInfo>> _group_members_info;

    int _self_uid = 0;
    int self_role = 0;
    QString _group_name = "";

    QMap<int, std::shared_ptr<ChatDataBase>> _msg_map;
    QMap<QString, std::shared_ptr<ChatDataBase>> _msg_unrsp_map;

    ChatFormType _thread_type = ChatFormType::PRIVATE;
};


#endif
