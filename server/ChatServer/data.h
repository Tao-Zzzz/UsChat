#pragma once
#include <string>
struct UserInfo {
	UserInfo():name(""), pwd(""),uid(0),email(""),nick(""),desc(""),sex(0), icon(""), back("") {}
	std::string name;
	std::string pwd;
	int uid;
	std::string email;
	std::string nick;
	std::string desc;
	int sex;
	std::string icon;
	std::string back;
};

struct ApplyInfo {
	ApplyInfo(int uid, std::string name, std::string desc,
		std::string icon, std::string nick, int sex, int status)
		:_uid(uid),_name(name),_desc(desc),
		_icon(icon),_nick(nick),_sex(sex),_status(status){}

	int _uid;
	std::string _name;
	std::string _desc;
	std::string _icon;
	std::string _nick;
	int _sex;
	int _status;
};


//�����߳���Ϣ
struct ChatThreadInfo {
	int _thread_id;
	std::string _type;     // "private" or "group"
	int _user1_id;    // ˽��ʱ��Ӧ private_chat.user1_id��Ⱥ��ʱ��Ϊ 0
	int _user2_id;    // ˽��ʱ��Ӧ private_chat.user2_id��Ⱥ��ʱ��Ϊ 0
};
