#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "DistLock.h"
#include <string>
#include "CServer.h"
#include "utils.h"
#include <vector>

using namespace std;

namespace
{
	bool ParseJsonSafe(const std::string& msg_data, Json::Value& root)
	{
		Json::Reader reader;
		return reader.parse(msg_data, root);
	}

	bool GetOnlineServerName(int uid, std::string& server_name)
	{
		std::string key = USERIPPREFIX + std::to_string(uid);
		return RedisMgr::GetInstance()->Get(key, server_name);
	}
}

LogicSystem::LogicSystem():_b_stop(false), _p_server(nullptr){
	RegisterCallBacks();
	_worker_thread = std::thread (&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem(){
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(shared_ptr < LogicNode> msg) {
	std::unique_lock<std::mutex> unique_lk(_mutex);
	_msg_que.push(msg);
	//由0变为1则发送通知信号
	if (_msg_que.size() == 1) {
		unique_lk.unlock();
		_consume.notify_one();
	}
}


void LogicSystem::SetServer(std::shared_ptr<CServer> pserver) {
	_p_server = pserver;
}


void LogicSystem::DealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		//判断队列为空则用条件变量阻塞等待，并释放锁
		while (_msg_que.empty() && !_b_stop) {
			_consume.wait(unique_lk);
		}

		//判断是否为关闭状态，把所有逻辑执行完后则退出循环
		if (_b_stop ) {
			while (!_msg_que.empty()) {
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter == _fun_callbacks.end()) {
					_msg_que.pop();
					continue;
				}
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
					std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
				_msg_que.pop();
			}
			break;
		}

		//如果没有停服，且说明队列中有数据
		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			std::cout << "msg id [" << msg_node->_recvnode->_msg_id << "] handler not found" << std::endl;
			continue;
		}
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id, 
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks() {
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::SearchInfo, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::AddFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_HEART_BEAT_REQ] = std::bind(&LogicSystem::HeartBeatHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_LOAD_CHAT_THREAD_REQ] = std::bind(&LogicSystem::GetUserThreadsHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	
	_fun_callbacks[ID_CREATE_PRIVATE_CHAT_REQ] = std::bind(&LogicSystem::CreatePrivateChat, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_LOAD_CHAT_MSG_REQ] = std::bind(&LogicSystem::LoadChatMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_IMG_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatImgMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CREATE_GROUP_REQ] = std::bind(&LogicSystem::CreateGroupChat, this,
		placeholders::_1, placeholders::_2, placeholders::_3);



	_fun_callbacks[ID_VIDEO_INVITE_REQ] = std::bind(&LogicSystem::VideoInvite, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_VIDEO_ACCEPT_REQ] = std::bind(&LogicSystem::VideoAccept, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_VIDEO_REJECT_REQ] = std::bind(&LogicSystem::VideoReject, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_VIDEO_CANCEL_REQ] = std::bind(&LogicSystem::VideoCancel, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_VIDEO_HANGUP_REQ] = std::bind(&LogicSystem::VideoHangup, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_WEBRTC_OFFER_REQ] = std::bind(&LogicSystem::WebrtcOffer, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_WEBRTC_ANSWER_REQ] = std::bind(&LogicSystem::WebrtcAnswer, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_WEBRTC_ICE_CANDIDATE_REQ] = std::bind(&LogicSystem::WebrtcIceCandidate, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	
}

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short &msg_id, const string &msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	std::cout << "user login uid is  " << uid << " user token  is "
		<< token << endl;

	Json::Value  rtvalue;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, MSG_CHAT_LOGIN_RSP);
		});


	//从redis获取用户token是否正确
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return ;
	}

	if (token_value != token) {
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return ;
	}

	rtvalue["error"] = ErrorCodes::Success;


	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	rtvalue["uid"] = uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	rtvalue["token"] = token;

	//从数据库获取申请列表
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply) {
		for (auto& apply : apply_list) {
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["nick"] = apply->_nick;
			obj["sex"] = apply->_sex;
			obj["desc"] = apply->_desc;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj);
		}
	}

	//获取好友列表
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	bool b_friend_list = GetFriendList(uid, friend_list);
	for (auto& friend_ele : friend_list) {
		Json::Value obj;
		obj["name"] = friend_ele->name;
		obj["uid"] = friend_ele->uid;
		obj["icon"] = friend_ele->icon;
		obj["nick"] = friend_ele->nick;
		obj["sex"] = friend_ele->sex;
		obj["desc"] = friend_ele->desc;
		obj["back"] = friend_ele->back;
		rtvalue["friend_list"].append(obj);
	}

	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
	{
		//此处添加分布式锁，让该线程独占登录
		//拼接用户ip对应的key
		auto lock_key = LOCK_PREFIX + uid_str;
		auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
		//利用defer解锁
		Defer defer2([this, identifier, lock_key]() {
			RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
			});
		//此处判断该用户是否在别处或者本服务器登录

		std::string uid_ip_value = "";
		auto uid_ip_key = USERIPPREFIX + uid_str;
		bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
		//说明用户已经登录了，此处应该踢掉之前的用户登录状态
		if (b_ip) {
			//获取当前服务器ip信息
			auto& cfg = ConfigMgr::Inst();
			auto self_name = cfg["SelfServer"]["Name"];
			//如果之前登录的服务器和当前相同，则直接在本服务器踢掉
			if (uid_ip_value == self_name) {
				//查找旧有的连接
				auto old_session = UserMgr::GetInstance()->GetSession(uid);

				//此处应该发送踢人消息
				if (old_session) {
					old_session->NotifyOffline(uid);
					//清除旧的连接
					_p_server->ClearSession(old_session->GetSessionId());
				}

			}
			else {
				//如果不是本服务器，则通知grpc通知其他服务器踢掉
				//发送通知
				KickUserReq kick_req;
				kick_req.set_uid(uid);
				ChatGrpcClient::GetInstance()->NotifyKickUser(uid_ip_value, kick_req);
			}
		}

		//session绑定用户uid
		session->SetUserId(uid);
		//为用户设置登录ip server的名字
		std::string  ipkey = USERIPPREFIX + uid_str;
		RedisMgr::GetInstance()->Set(ipkey, server_name);
		//uid和session绑定管理,方便以后踢人操作
		UserMgr::GetInstance()->SetUserSession(uid, session);
		std::string  uid_session_key = USER_SESSION_PREFIX + uid_str;
		RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId());

	}

	return;
}

void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();
	std::cout << "user SearchInfo uid is  " << uid_str << endl;

	Json::Value  rtvalue;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP);
		});

	bool b_digit = isPureDigit(uid_str);
	if (b_digit) {
		GetUserByUid(uid_str, rtvalue);
	}
	else {
		GetUserByName(uid_str, rtvalue);
	}
	return;
}

void LogicSystem::AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto desc = root["applyname"].asString();
	auto bakname = root["bakname"].asString();
	auto touid = root["touid"].asInt();
	std::cout << "user login uid is  " << uid << " applydesc  is "
		<< desc << " bakname is " << bakname << " touid is " << touid << endl;

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_ADD_FRIEND_RSP);
		});

	//先更新数据库
	MysqlMgr::GetInstance()->AddFriendApply(uid, touid, desc, bakname);

	//查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}


	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];


	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	//直接通知对方有申请消息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中则直接发送通知对方
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["applyuid"] = uid;
			notify["name"] = apply_info->name;
			notify["desc"] = desc;
			if (b_info) {
				notify["icon"] = apply_info->icon;
				notify["sex"] = apply_info->sex;
				notify["nick"] = apply_info->nick;
			}
			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
		}

		return ;
	}

	
	AddFriendReq add_req;
	add_req.set_applyuid(uid);
	add_req.set_touid(touid);
	add_req.set_name(apply_info->name);
	add_req.set_desc(desc);
	if (b_info) {
		add_req.set_icon(apply_info->icon);
		add_req.set_sex(apply_info->sex);
		add_req.set_nick(apply_info->nick);
	}

	//发送通知
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value,add_req);

}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto back_name = root["back"].asString();
	std::cout << "from " << uid << " auth friend to " << touid << std::endl;

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	auto user_info = std::make_shared<UserInfo>();

	std::string base_key = USER_BASE_INFO + std::to_string(touid);
	bool b_info = GetBaseInfo(base_key, touid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
		rtvalue["uid"] = touid;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}


	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_AUTH_FRIEND_RSP);
		});

	//先更新数据库， 放到事务中，此处不再处理
	//MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);

	std::vector<std::shared_ptr<AddFriendMsg>> chat_datas;

	//更新数据库添加好友
	MysqlMgr::GetInstance()->AddFriend(uid, touid,back_name, chat_datas);

	//查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//直接通知对方有认证通过消息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中则直接发送通知对方
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["fromuid"] = uid;
			notify["touid"] = touid;
			std::string base_key = USER_BASE_INFO + std::to_string(uid);
			auto user_info = std::make_shared<UserInfo>();
			bool b_info = GetBaseInfo(base_key, uid, user_info);
			if (b_info) {
				notify["name"] = user_info->name;
				notify["nick"] = user_info->nick;
				notify["icon"] = user_info->icon;
				notify["sex"] = user_info->sex;
			}
			else {
				notify["error"] = ErrorCodes::UidInvalid;
			}

			auto chat_time = getCurrentTimestamp();
			for(auto & chat_data : chat_datas)
			{
				Json::Value  chat;
				chat["sender"] = chat_data->sender_id();
				chat["msg_id"] = chat_data->msg_id();
				chat["thread_id"] = chat_data->thread_id();
				chat["unique_id"] = chat_data->unique_id();
				chat["msg_content"] = chat_data->msgcontent();
				chat["chat_time"] = chat_time;
				chat["status"] = chat_data->status();
				notify["chat_datas"].append(chat);
				rtvalue["chat_datas"].append(chat);
			}

			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
		}

		return ;
	}


	AuthFriendReq auth_req;
	auth_req.set_fromuid(uid);
	auth_req.set_touid(touid);
	auto chat_time = getCurrentTimestamp();
	for(auto& chat_data : chat_datas)
	{
		auto text_msg = auth_req.add_textmsgs();
		text_msg->CopyFrom(*chat_data);
		Json::Value  chat;
		chat["sender"] = chat_data->sender_id();
		chat["msg_id"] = chat_data->msg_id();
		chat["thread_id"] = chat_data->thread_id();
		chat["unique_id"] = chat_data->unique_id();
		chat["msg_content"] = chat_data->msgcontent();
		chat["chat_time"] = chat_time;
		chat["status"] = chat_data->status();
		rtvalue["chat_datas"].append(chat);
	}
	//发送通知
	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

//void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
//	Json::Reader reader;
//	Json::Value root;
//	reader.parse(msg_data, root);
//
//	auto uid = root["fromuid"].asInt();
//	auto touid = root["touid"].asInt();
//
//	const Json::Value  arrays = root["text_array"];
//	
//	Json::Value  rtvalue;
//	rtvalue["error"] = ErrorCodes::Success;
//
//	rtvalue["fromuid"] = uid;
//	rtvalue["touid"] = touid;
//	auto thread_id = root["thread_id"].asInt();
//	rtvalue["thread_id"] = thread_id;
//	std::vector<std::shared_ptr<ChatMessage>> chat_datas;
//	auto timestamp = getCurrentTimestamp();
//	for (const auto& txt_obj : arrays) {
//		auto content = txt_obj["content"].asString();
//		auto unique_id = txt_obj["unique_id"].asString();
//		std::cout << "content is " << content << std::endl;
//		std::cout << "unique_id is " << unique_id << std::endl;
//		auto chat_msg = std::make_shared<ChatMessage>();
//		chat_msg->chat_time = timestamp;
//		chat_msg->sender_id = uid;
//		chat_msg->recv_id = touid;
//		chat_msg->unique_id = unique_id;
//		chat_msg->thread_id = thread_id;
//		chat_msg->content = content;
//		chat_msg->status = 2;
//		chat_datas.push_back(chat_msg);
//	}
//
//
//	//插入数据库
//	MysqlMgr::GetInstance()->AddChatMsg(chat_datas);
//
//
//	for (const auto& chat_data : chat_datas) {
//		Json::Value  chat_msg;
//		chat_msg["message_id"] = chat_data->message_id;
//		chat_msg["unique_id"] = chat_data->unique_id;
//		chat_msg["content"] = chat_data->content;
//		chat_msg["status"] = chat_data->status;
//		chat_msg["chat_time"] = chat_data->chat_time;
//		rtvalue["chat_datas"].append(chat_msg);
//	}
//
//	Defer defer([this, &rtvalue, session]() {
//		std::string return_str = rtvalue.toStyledString();
//		session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
//		});
//
//
//	//查询redis 查找touid对应的server ip
//	auto to_str = std::to_string(touid);
//	auto to_ip_key = USERIPPREFIX + to_str;
//	std::string to_ip_value = "";
//	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
//	if (!b_ip) {
//		return;
//	}
//
//	auto& cfg = ConfigMgr::Inst();
//	auto self_name = cfg["SelfServer"]["Name"];
//	//直接通知对方有认证通过消息
//	if (to_ip_value == self_name) {
//		auto session = UserMgr::GetInstance()->GetSession(touid);
//		if (session) {
//			//在内存中则直接发送通知对方
//			std::string return_str = rtvalue.toStyledString();
//			session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
//		}
//
//		return ;
//	}
//
//
//	TextChatMsgReq text_msg_req;
//	text_msg_req.set_fromuid(uid);
//	text_msg_req.set_touid(touid);
//	text_msg_req.set_thread_id(thread_id);
//	for (const auto& chat_data : chat_datas) {
//		auto *text_msg = text_msg_req.add_textmsgs();
//		text_msg->set_unique_id(chat_data->unique_id);
//		text_msg->set_msgcontent(chat_data->content);
//		text_msg->set_msg_id(chat_data->message_id);
//		text_msg->set_chat_time(chat_data->chat_time);
//	}
//
//
//	//发送通知 todo...
//	ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
//}


void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto fromuid = root["fromuid"].asInt();
	auto thread_id = root["thread_id"].asInt();
	const Json::Value arrays = root["text_array"];
	auto touid = root["touid"].asInt();
	// 1. 解析所有的接收者
	std::vector<int> receiver_uids;
	if (touid == 0) {
		for (const auto& id_val : root["to_member_uids"]) {
			receiver_uids.push_back(id_val.asInt());
		}
	}
	else {
		// 兼容单聊旧逻辑
		receiver_uids.push_back(root["touid"].asInt());
	}

	// 2. 构造消息体并存库 (只存一次)
	std::vector<std::shared_ptr<ChatMessage>> chat_datas;
	auto timestamp = getCurrentTimestamp();
	for (const auto& txt_obj : arrays) {
		auto chat_msg = std::make_shared<ChatMessage>();
		chat_msg->chat_time = timestamp;
		chat_msg->sender_id = fromuid;
		// 如果是群聊，数据库里的 recv_id 可以设为 0，靠 thread_id 区分
		chat_msg->recv_id = (touid == 0) ? 0 : touid;
		chat_msg->unique_id = txt_obj["unique_id"].asString();
		chat_msg->thread_id = thread_id;
		chat_msg->content = txt_obj["content"].asString();
		chat_msg->status = 2;
		chat_datas.push_back(chat_msg);
	}

	MysqlMgr::GetInstance()->AddChatMsg(chat_datas);

	// 3. 构造返回给发送者的回执 (RSP)
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = fromuid;
	rtvalue["touid"] = touid;
	rtvalue["thread_id"] = thread_id;

	for (const auto& chat_data : chat_datas) {
		Json::Value msg_node;
		msg_node["message_id"] = chat_data->message_id;
		msg_node["unique_id"] = chat_data->unique_id;
		msg_node["content"] = chat_data->content;
		msg_node["status"] = chat_data->status;
		msg_node["chat_time"] = chat_data->chat_time;
		rtvalue["chat_datas"].append(msg_node);
	}

	// 回复发送者
	Defer defer([this, &rtvalue, session]() {
		session->Send(rtvalue.toStyledString(), ID_TEXT_CHAT_MSG_RSP);
		});

	std::unordered_map<std::string, std::vector<int>> notify_map;
	// 4. 遍历接收者列表，发送通知 (NOTIFY)
	for (int current_touid : receiver_uids) {
		// 过滤掉发送者自己
		if (current_touid == fromuid) continue;

		// 查询 Redis 查找当前接收者对应的 server ip
		auto to_str = std::to_string(current_touid);
		auto to_ip_key = USERIPPREFIX + to_str;
		std::string to_ip_value = "";
		bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
		if (!b_ip) continue; // 用户不在线

		// 准备转发给接收者的 Json 结构
		Json::Value notify_value = rtvalue;
		notify_value["touid"] = current_touid; // 这里的 touid 设置为当前接收者

		auto& cfg = ConfigMgr::Inst();
		auto self_name = cfg["SelfServer"]["Name"];


		// 情况 A: 目标用户在当前服务器
		if (to_ip_value == self_name) {
			auto target_session = UserMgr::GetInstance()->GetSession(current_touid);
			if (target_session) {
				target_session->Send(notify_value.toStyledString(), ID_NOTIFY_TEXT_CHAT_MSG_REQ);
			}
			continue;
		}

		// 情况 B: 目标用户在其他服务器，走 gRPC 转发
		
		notify_map[to_ip_value].push_back(current_touid);


	}

	for (auto it = notify_map.begin(); it != notify_map.end(); ++it) {
		auto& server_ip = it->first;
		auto& uids = it->second;

		GroupTextChatMsgReq req;
		req.set_fromuid(fromuid);
		req.set_thread_id(thread_id);
		for (auto uid : uids) req.add_touids(uid);

		for (const auto& chat_data : chat_datas) {
			auto* text_msg = req.add_textmsgs();
			text_msg->set_unique_id(chat_data->unique_id);
			text_msg->set_msgcontent(chat_data->content);
			text_msg->set_msg_id(chat_data->message_id);
			text_msg->set_chat_time(chat_data->chat_time);
		}
		std::cout << "-------------------------------------------------------------------" << server_ip << std::endl;
		std::cout << "sending grpc group chat to server_ip: " << server_ip << std::endl;
		ChatGrpcClient::GetInstance()->NotifyGroupTextChatMsg(server_ip, req, rtvalue);
	}
}

void LogicSystem::HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["fromuid"].asInt();
	std::cout << "receive heart beat msg, uid is " << uid << std::endl;
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	session->Send(rtvalue.toStyledString(), ID_HEARTBEAT_RSP);
}

bool LogicSystem::isPureDigit(const std::string& str)
{
	for (char c : str) {
		if (!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;

	//优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " pwd is " << pwd << " email is " << email <<" icon is " << icon << endl;

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	auto uid = std::stoi(uid_str);
	//redis中没有则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	//将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	//返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}

void LogicSystem::GetUserByName(std::string name, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = NAME_INFO + name;

	//优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " pwd is " << pwd << " email is " << email << endl;

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	//redis中没有则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	//将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	
	//返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}

bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	//优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		std::cout << "user login uid is  " << userinfo->uid << " name  is "
			<< userinfo->name << " pwd is " << userinfo->pwd << " email is " << userinfo->email << endl;
	}
	else {
		//redis中没有则查询mysql
		//查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			return false;
		}

		userinfo = user_info;

		//将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	}

	return true;
}

bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list) {
	//从mysql获取好友申请列表
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	//从mysql获取好友列表
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}

void LogicSystem::GetUserThreadsHandler(std::shared_ptr<CSession> session, 
	const short& msg_id, const string& msg_data)
{
	//从数据库加chat_threads记录
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	int last_id = root["thread_id"].asInt();
	std::cout << "get uid  threads  " << uid << std::endl;

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_LOAD_CHAT_THREAD_RSP);
		});
	
	std::vector<std::shared_ptr<ChatThreadInfo>> threads;
	
	int page_size = 10;
	bool load_more = false;
	int next_last_id = 0;
	bool res = GetUserThreads(uid, last_id, page_size, threads, load_more, next_last_id);
	if (!res) {
		rtvalue["error"] = ErrorCodes::UidInvalid;                                 
		return;
	}

	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["load_more"] = load_more;
	rtvalue["next_last_id"] = (int)next_last_id;

	Json::Value threads_value(Json::arrayValue);

	for (auto& thread : threads) {
		Json::Value thread_value;
		thread_value["thread_id"] = (int)thread->_thread_id;
		thread_value["type"] = thread->_type;

		// 区分会话类型进行打包
		if (thread->_type == "private") {
			thread_value["user1_id"] = (int)thread->_user1_id;
			thread_value["user2_id"] = (int)thread->_user2_id;
		}
		else if (thread->_type == "group") {
			// 添加群聊名称
			thread_value["group_name"] = thread->_group_name;

			Json::Value members_value(Json::arrayValue);
			for (auto& kv : thread->_meber_infos) {
				int uid = kv.first;
				auto& info = kv.second;

				Json::Value mem;
				mem["uid"] = uid;
				mem["role"] = info->_role;
				mem["mute_until"] = info->_mute_until;
				mem["group_nick"] = info->_group_nickname;

				members_value.append(mem);
			}
			thread_value["members"] = members_value;
		}

		threads_value.append(thread_value);
	}


	rtvalue["threads"] = threads_value;

}

bool LogicSystem::GetUserThreads(int64_t userId,
	int64_t lastId,
	int      pageSize,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads,
	bool& loadMore,
	int& nextLastId)
{
	return MysqlMgr::GetInstance()->GetUserThreads(userId, lastId, pageSize, 
		threads, loadMore, nextLastId);
}

void LogicSystem::CreatePrivateChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto other_id = root["other_id"].asInt();
	
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["other_id"] = other_id;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CREATE_PRIVATE_CHAT_RSP);
		});

	int thread_id = 0;
	bool res = MysqlMgr::GetInstance()->CreatePrivateChat(uid, other_id, thread_id);
	if (!res) {
		rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
		return;
	}

	rtvalue["thread_id"] = thread_id;
}

void LogicSystem::LoadChatMsg(std::shared_ptr<CSession> session, 
	const short& msg_id, const string& msg_data) {

	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto thread_id = root["thread_id"].asInt();
	auto message_id = root["message_id"].asInt();


	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["thread_id"] = thread_id;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_LOAD_CHAT_MSG_RSP);
		});

	int page_size = 10;
	std::shared_ptr<PageResult> res = MysqlMgr::GetInstance()->LoadChatMsg(thread_id, message_id, page_size);
	if (!res) {
		rtvalue["error"] = ErrorCodes::LOAD_CHAT_FAILED;
		return;
	}

	rtvalue["last_message_id"] = res->next_cursor;
	rtvalue["load_more"] = res->load_more;
	for (auto& chat : res->messages) {
		Json::Value  chat_data;
		chat_data["sender"] = chat.sender_id;
		chat_data["msg_id"] = chat.message_id;
		chat_data["thread_id"] = chat.thread_id;
		chat_data["unique_id"] = 0;
		chat_data["msg_content"] = chat.content;
		chat_data["chat_time"] = chat.chat_time;
		chat_data["status"] = chat.status;
		chat_data["msg_type"] = chat.msg_type;
		chat_data["receiver"] = chat.recv_id;
		rtvalue["chat_datas"].append(chat_data);
	}

}


void LogicSystem::DealChatImgMsg(std::shared_ptr<CSession> session,
	const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();

	auto md5 = root["md5"].asString();
	auto unique_name = root["name"].asString();
	auto token = root["token"].asString();
	auto unique_id = root["unique_id"].asString();
	auto chat_time = root["chat_time"].asString();
	auto status = root["status"].asInt();

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	auto thread_id = root["thread_id"].asInt();
	rtvalue["thread_id"] = thread_id;
	rtvalue["md5"] = md5;
	rtvalue["unique_name"] = unique_name;
	rtvalue["unique_id"] = unique_id;
	rtvalue["chat_time"] = chat_time;
	rtvalue["status"] = MsgStatus::UN_UPLOAD;

	auto timestamp = getCurrentTimestamp();
	auto chat_msg = std::make_shared<ChatMessage>();
	chat_msg->chat_time = timestamp;
	chat_msg->sender_id = uid;
	chat_msg->recv_id = touid;
	chat_msg->unique_id = unique_id;
	chat_msg->thread_id = thread_id;
	chat_msg->content = unique_name;
	chat_msg->status = MsgStatus::UN_UPLOAD;
	chat_msg->msg_type = int(ChatMsgType::PIC);

	//插入数据库
	MysqlMgr::GetInstance()->AddChatMsg(chat_msg);

	rtvalue["message_id"] = chat_msg->message_id;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_IMG_CHAT_MSG_RSP);
		});

}

static Json::Value BuildGroupChatJson(const GroupChatInfo& group_info)
{
	Json::Value rtvalue;
	rtvalue["error"] = 0;
	rtvalue["thread_id"] = group_info.thread_id;
	rtvalue["group_name"] = group_info.group_name;
	rtvalue["member_count"] = group_info.member_count;

	Json::Value members(Json::arrayValue);
	for (const auto& m : group_info.members)
	{
		Json::Value item;
		item["uid"] = m.uid;
		item["name"] = m.name;
		item["role"] = m.role;
		members.append(item);
	}

	rtvalue["members"] = members;
	return rtvalue;
}

void LogicSystem::CreateGroupChat(std::shared_ptr<CSession> session,
	const short& msg_id,
	const std::string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	int uid = root["uid"].asInt();

	std::vector<int> other_members;
	const Json::Value& members = root["other_member"];
	if (members.isArray())
	{
		for (int i = 0; i < members.size(); ++i)
		{
			other_members.push_back(members[i].asInt());
		}
	}

	Json::Value rtvalue;
	int thread_id = 0;

	bool res = MysqlMgr::GetInstance()->CreateGroupChat(uid, other_members, thread_id);
	if (!res)
	{
		rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
		session->Send(rtvalue.toStyledString(), ID_CREATE_GROUP_RSP);
		return;
	}

	GroupChatInfo group_info;
	res = MysqlMgr::GetInstance()->GetGroupChatInfo(thread_id, group_info);
	if (!res)
	{
		rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
		session->Send(rtvalue.toStyledString(), ID_CREATE_GROUP_RSP);
		return;
	}

	rtvalue = BuildGroupChatJson(group_info);

	Defer defer([session, &rtvalue]() {
		session->Send(rtvalue.toStyledString(), ID_CREATE_GROUP_RSP);
		});

	// 收集所有成员 uid
	std::vector<int> all_members;
	for (const auto& m : group_info.members)
	{
		all_members.push_back(m.uid);
	}

	std::unordered_map<std::string, std::vector<int>> notify_map;

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	for (auto member_uid : all_members)
	{
		if (member_uid == uid) continue; // 跳过创建者自己

		std::string to_ip_value;
		auto key = USERIPPREFIX + std::to_string(member_uid);

		if (!RedisMgr::GetInstance()->Get(key, to_ip_value))
		{
			continue;
		}

		Json::Value notify_value = rtvalue;
		notify_value["touid"] = member_uid;

		if (to_ip_value == self_name)
		{
			auto target_session = UserMgr::GetInstance()->GetSession(member_uid);
			if (target_session)
			{
				target_session->Send(
					notify_value.toStyledString(),
					ID_NOTIFY_GROUP_CREATED_REQ);
			}
		}
		else
		{
			notify_map[to_ip_value].push_back(member_uid);
		}
	}

	// 跨服通知
	for (auto& kv : notify_map)
	{
		const std::string& server_ip = kv.first;
		const std::vector<int>& touids = kv.second;

		GroupCreatedNotifyReq req;
		req.set_thread_id(group_info.thread_id);
		req.set_group_name(group_info.group_name);
		req.set_member_count(group_info.member_count);

		for (int touid : touids)
		{
			req.add_touids(touid);
		}

		for (const auto& m : group_info.members)
		{
			auto* member = req.add_members();
			member->set_uid(m.uid);
			member->set_name(m.name);
			member->set_role(m.role);
		}

		ChatGrpcClient::GetInstance()->NotifyGroupCreated(server_ip, req);
	}
}


// --- 辅助清理函数（用于结束通话时清除 Redis 状态） ---
void LogicSystem::ClearCallSessionInRedis(const std::string& call_id, int caller_id, int callee_id) {
	RedisMgr::GetInstance()->Del(CALL_SESSION_PREFIX + call_id);
	RedisMgr::GetInstance()->Del(USER_BUSY_PREFIX + std::to_string(caller_id));
	RedisMgr::GetInstance()->Del(USER_BUSY_PREFIX + std::to_string(callee_id));
}


void LogicSystem::VideoInvite(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([session, &rtvalue]() {
		session->Send(rtvalue.toStyledString(), ID_VIDEO_INVITE_RSP);
		});

	if (!ParseJsonSafe(msg_data, root)) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	if (!root.isMember("uid") || !root.isMember("other_id") || !root.isMember("call_type")) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	int call_type = root["call_type"].asInt();

	if (uid <= 0 || other_id <= 0 || call_type < 0) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	if (uid == other_id) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_SELF_CALL;
		return;
	}

	// ===== 跨服分布式占线检测 =====
	std::string caller_busy_key = USER_BUSY_PREFIX + std::to_string(uid);
	std::string callee_busy_key = USER_BUSY_PREFIX + std::to_string(other_id);
	if (RedisMgr::GetInstance()->ExistsKey(caller_busy_key) || RedisMgr::GetInstance()->ExistsKey(callee_busy_key)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_USER_BUSY;
		return;
	}

	std::string to_server_name;
	if (!GetOnlineServerName(other_id, to_server_name)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_USER_OFFLINE;
		return;
	}

	// ===== 动态生成 call_id =====
	auto now = std::chrono::system_clock::now();
	auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	std::string call_id = std::to_string(uid) + "-" + std::to_string(other_id) + "-" + std::to_string(timestamp);

	// ===== 将通话会话存入 Redis =====
	Json::Value session_json;
	session_json["call_id"] = call_id;
	session_json["caller_id"] = uid;
	session_json["callee_id"] = other_id;
	session_json["call_type"] = call_type;
	session_json["status"] = "calling";
	std::string session_str = session_json.toStyledString();

	std::string session_key = CALL_SESSION_PREFIX + call_id;
	// 使用新加的 SetEx 设置 180 秒过期
	RedisMgr::GetInstance()->SetEx(session_key, session_str, CALL_SESSION_TTL);
	RedisMgr::GetInstance()->SetEx(caller_busy_key, call_id, CALL_SESSION_TTL);
	RedisMgr::GetInstance()->SetEx(callee_busy_key, call_id, CALL_SESSION_TTL);

	rtvalue["call_id"] = call_id;
	rtvalue["other_id"] = other_id;
	rtvalue["call_type"] = call_type;

	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	Json::Value notify;
	notify["error"] = ErrorCodes::Success;
	notify["call_id"] = call_id;
	notify["from_uid"] = uid;
	notify["call_type"] = call_type;

	if (b_info) {
		notify["name"] = apply_info->name;
		notify["icon"] = apply_info->icon;
		notify["nick"] = apply_info->nick;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	// 同服直接发送
	if (to_server_name == self_name) {
		auto to_session = UserMgr::GetInstance()->GetSession(other_id);
		if (!to_session) {
			ClearCallSessionInRedis(call_id, uid, other_id);
			rtvalue["error"] = ErrorCodes::ERR_VIDEO_USER_OFFLINE;
			rtvalue.removeMember("call_id");
			rtvalue.removeMember("other_id");
			rtvalue.removeMember("call_type");
			return;
		}

		to_session->Send(notify.toStyledString(), ID_NOTIFY_VIDEO_INVITE_REQ);
		return;
	}

	// ===== 跨服 gRPC 发送 =====
	auto to_str = std::to_string(other_id);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);

	if (!b_ip) {
		ClearCallSessionInRedis(call_id, uid, other_id);
		rtvalue["error"] = ErrorCodes::RPCFailed;
		rtvalue.removeMember("call_id");
		rtvalue.removeMember("other_id");
		rtvalue.removeMember("call_type");
		return;
	}

	NotifyVideoEventReq req;
	req.set_from_uid(uid);
	req.set_to_uid(other_id);
	req.set_call_id(call_id);
	req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_INVITE);
	req.set_call_type(call_type);

	if (b_info) {
		req.set_name(apply_info->name);
		req.set_icon(apply_info->icon);
		req.set_nick(apply_info->nick);
	}

	auto rsp = ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
	if (rsp.error() != ErrorCodes::Success) {
		ClearCallSessionInRedis(call_id, uid, other_id);
		rtvalue["error"] = rsp.error();
		rtvalue.removeMember("call_id");
		rtvalue.removeMember("other_id");
		rtvalue.removeMember("call_type"); // 此处原代码有个 media_type typo，已修正为 call_type
		return;
	}
}

void LogicSystem::VideoAccept(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([session, &rtvalue]() {
		session->Send(rtvalue.toStyledString(), ID_VIDEO_ACCEPT_RSP);
		});

	if (!ParseJsonSafe(msg_data, root)) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	if (!root.isMember("uid") || !root.isMember("other_id") || !root.isMember("call_id")) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	std::string call_id = root["call_id"].asString();

	// ===== 从 Redis 获取会话并校验 =====
	std::string session_key = CALL_SESSION_PREFIX + call_id;
	std::string session_str;
	if (!RedisMgr::GetInstance()->Get(session_key, session_str)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}

	Json::Value session_json;
	if (!ParseJsonSafe(session_str, session_json)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}

	int caller_id = session_json["caller_id"].asInt();
	int callee_id = session_json["callee_id"].asInt();

	// IsSameCallPair & IsCallee 验证
	if ((uid != caller_id && uid != callee_id) || (other_id != caller_id && other_id != callee_id) || uid != callee_id) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}

	// 更新 Redis 状态并刷新 TTL (可选，确保通话中不会过期)
	session_json["status"] = "accepted";
	RedisMgr::GetInstance()->SetEx(session_key, session_json.toStyledString(), CALL_SESSION_TTL * 10); // 比如续期半小时

	rtvalue["call_id"] = call_id;

	Json::Value notify;
	notify["error"] = ErrorCodes::Success;
	notify["call_id"] = call_id;
	notify["uid"] = uid;

	std::string to_server_name;
	if (!GetOnlineServerName(other_id, to_server_name)) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	if (to_server_name == self_name) {
		auto to_session = UserMgr::GetInstance()->GetSession(other_id);
		if (to_session) {
			to_session->Send(notify.toStyledString(), ID_NOTIFY_VIDEO_ACCEPT_REQ);
		}
		return;
	}

	auto to_str = std::to_string(other_id);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	if (!RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value)) {
		rtvalue["error"] = ErrorCodes::RPCFailed;
		return;
	}

	NotifyVideoEventReq req;
	req.set_from_uid(uid);
	req.set_to_uid(other_id);
	req.set_call_id(call_id);
	req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_ACCEPT);

	auto rsp = ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
	if (rsp.error() != ErrorCodes::Success) {
		rtvalue["error"] = rsp.error();
	}
}

void LogicSystem::VideoReject(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([session, &rtvalue]() {
		session->Send(rtvalue.toStyledString(), ID_VIDEO_REJECT_RSP);
		});

	if (!ParseJsonSafe(msg_data, root)) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}
	if (!root.isMember("uid") || !root.isMember("other_id") || !root.isMember("call_id")) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	std::string call_id = root["call_id"].asString();

	// ===== 从 Redis 获取会话并校验 =====
	std::string session_str;
	if (!RedisMgr::GetInstance()->Get(CALL_SESSION_PREFIX + call_id, session_str)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}
	Json::Value session_json;
	ParseJsonSafe(session_str, session_json);
	int caller_id = session_json["caller_id"].asInt();
	int callee_id = session_json["callee_id"].asInt();

	if ((uid != caller_id && uid != callee_id) || (other_id != caller_id && other_id != callee_id) || uid != callee_id) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}

	Json::Value notify;
	notify["error"] = ErrorCodes::Success;
	notify["call_id"] = call_id;
	notify["uid"] = uid;

	std::string to_server_name;
	if (GetOnlineServerName(other_id, to_server_name)) {
		auto& cfg = ConfigMgr::Inst();
		if (to_server_name == cfg["SelfServer"]["Name"]) {
			auto to_session = UserMgr::GetInstance()->GetSession(other_id);
			if (to_session) to_session->Send(notify.toStyledString(), ID_NOTIFY_VIDEO_REJECT_REQ);
		}
		else {
			std::string to_ip_value = "";
			if (RedisMgr::GetInstance()->Get(USERIPPREFIX + std::to_string(other_id), to_ip_value)) {
				NotifyVideoEventReq req;
				req.set_from_uid(uid);
				req.set_to_uid(other_id);
				req.set_call_id(call_id);
				req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_REJECT);
				auto rsp = ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
				if (rsp.error() != ErrorCodes::Success) rtvalue["error"] = rsp.error();
			}
			else {
				rtvalue["error"] = ErrorCodes::RPCFailed;
			}
		}
	}

	// 通话结束，清理 Redis 占用
	ClearCallSessionInRedis(call_id, caller_id, callee_id);
}

void LogicSystem::VideoCancel(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([session, &rtvalue]() {
		session->Send(rtvalue.toStyledString(), ID_VIDEO_CANCEL_RSP);
		});

	if (!ParseJsonSafe(msg_data, root) || !root.isMember("uid") || !root.isMember("other_id") || !root.isMember("call_id")) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	std::string call_id = root["call_id"].asString();

	// ===== 从 Redis 获取会话并校验 =====
	std::string session_str;
	if (!RedisMgr::GetInstance()->Get(CALL_SESSION_PREFIX + call_id, session_str)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}
	Json::Value session_json;
	ParseJsonSafe(session_str, session_json);
	int caller_id = session_json["caller_id"].asInt();
	int callee_id = session_json["callee_id"].asInt();

	if ((uid != caller_id && uid != callee_id) || (other_id != caller_id && other_id != callee_id) || uid != caller_id) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}

	Json::Value notify;
	notify["error"] = ErrorCodes::Success;
	notify["call_id"] = call_id;
	notify["uid"] = uid;

	std::string to_server_name;
	if (GetOnlineServerName(other_id, to_server_name)) {
		auto& cfg = ConfigMgr::Inst();
		if (to_server_name == cfg["SelfServer"]["Name"]) {
			auto to_session = UserMgr::GetInstance()->GetSession(other_id);
			if (to_session) to_session->Send(notify.toStyledString(), ID_NOTIFY_VIDEO_CANCEL_REQ);
		}
		else {
			std::string to_ip_value = "";
			if (RedisMgr::GetInstance()->Get(USERIPPREFIX + std::to_string(other_id), to_ip_value)) {
				NotifyVideoEventReq req;
				req.set_from_uid(uid);
				req.set_to_uid(other_id);
				req.set_call_id(call_id);
				req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_CANCEL);
				auto rsp = ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
				if (rsp.error() != ErrorCodes::Success) rtvalue["error"] = rsp.error();
			}
			else {
				rtvalue["error"] = ErrorCodes::RPCFailed;
			}
		}
	}

	// 通话结束，清理 Redis 占用
	ClearCallSessionInRedis(call_id, caller_id, callee_id);
}

void LogicSystem::VideoHangup(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([session, &rtvalue]() {
		session->Send(rtvalue.toStyledString(), ID_VIDEO_HANGUP_RSP);
		});

	if (!ParseJsonSafe(msg_data, root) || !root.isMember("uid") || !root.isMember("other_id") || !root.isMember("call_id")) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	std::string call_id = root["call_id"].asString();

	// ===== 从 Redis 获取会话并校验 =====
	std::string session_str;
	if (!RedisMgr::GetInstance()->Get(CALL_SESSION_PREFIX + call_id, session_str)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}
	Json::Value session_json;
	ParseJsonSafe(session_str, session_json);
	int caller_id = session_json["caller_id"].asInt();
	int callee_id = session_json["callee_id"].asInt();

	if ((uid != caller_id && uid != callee_id) || (other_id != caller_id && other_id != callee_id)) {
		rtvalue["error"] = ErrorCodes::ERR_VIDEO_CALL_NOT_FOUND;
		return;
	}

	Json::Value notify;
	notify["error"] = ErrorCodes::Success;
	notify["call_id"] = call_id;
	notify["uid"] = uid;

	std::string to_server_name;
	if (GetOnlineServerName(other_id, to_server_name)) {
		auto& cfg = ConfigMgr::Inst();
		if (to_server_name == cfg["SelfServer"]["Name"]) {
			auto to_session = UserMgr::GetInstance()->GetSession(other_id);
			if (to_session) to_session->Send(notify.toStyledString(), ID_NOTIFY_VIDEO_HANGUP_REQ);
		}
		else {
			std::string to_ip_value = "";
			if (RedisMgr::GetInstance()->Get(USERIPPREFIX + std::to_string(other_id), to_ip_value)) {
				NotifyVideoEventReq req;
				req.set_from_uid(uid);
				req.set_to_uid(other_id);
				req.set_call_id(call_id);
				req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_HANGUP);
				auto rsp = ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
				if (rsp.error() != ErrorCodes::Success) rtvalue["error"] = rsp.error();
			}
			else {
				rtvalue["error"] = ErrorCodes::RPCFailed;
			}
		}
	}

	// 通话结束，清理 Redis 占用
	ClearCallSessionInRedis(call_id, caller_id, callee_id);
}

void LogicSystem::WebrtcOffer(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	if (!ParseJsonSafe(msg_data, root) || !root.isMember("uid") || !root.isMember("other_id") ||
		!root.isMember("call_id") || !root.isMember("sdp")) {
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	std::string call_id = root["call_id"].asString();
	std::string sdp = root["sdp"].asString();

	if (uid <= 0 || other_id <= 0 || call_id.empty() || sdp.empty()) return;

	// ===== 从 Redis 获取会话并校验 =====
	std::string session_str;
	if (!RedisMgr::GetInstance()->Get(CALL_SESSION_PREFIX + call_id, session_str)) return;

	Json::Value session_json;
	ParseJsonSafe(session_str, session_json);
	int caller_id = session_json["caller_id"].asInt();
	int callee_id = session_json["callee_id"].asInt();

	if ((uid != caller_id && uid != callee_id) || uid != caller_id) return; // 只有发起方能发 Offer

	std::string to_server_name;
	if (!GetOnlineServerName(other_id, to_server_name)) return;

	auto& cfg = ConfigMgr::Inst();
	if (to_server_name == cfg["SelfServer"]["Name"]) {
		auto to_session = UserMgr::GetInstance()->GetSession(other_id);
		if (to_session) to_session->Send(root.toStyledString(), ID_NOTIFY_WEBRTC_OFFER_REQ);
		return;
	}

	std::string to_ip_value = "";
	if (!RedisMgr::GetInstance()->Get(USERIPPREFIX + std::to_string(other_id), to_ip_value)) return;

	NotifyVideoEventReq req;
	req.set_from_uid(uid);
	req.set_to_uid(other_id);
	req.set_call_id(call_id);
	req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_WEBRTC_OFFER);
	req.set_sdp(sdp);

	ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
}

void LogicSystem::WebrtcAnswer(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	if (!ParseJsonSafe(msg_data, root) || !root.isMember("uid") || !root.isMember("other_id") ||
		!root.isMember("call_id") || !root.isMember("sdp")) {
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	std::string call_id = root["call_id"].asString();
	std::string sdp = root["sdp"].asString();

	if (uid <= 0 || other_id <= 0 || call_id.empty() || sdp.empty()) return;

	// ===== 从 Redis 获取会话并校验 =====
	std::string session_str;
	if (!RedisMgr::GetInstance()->Get(CALL_SESSION_PREFIX + call_id, session_str)) return;

	Json::Value session_json;
	ParseJsonSafe(session_str, session_json);
	int caller_id = session_json["caller_id"].asInt();
	int callee_id = session_json["callee_id"].asInt();

	if ((uid != caller_id && uid != callee_id) || uid != callee_id) return; // 只有接收方能发 Answer

	std::string to_server_name;
	if (!GetOnlineServerName(other_id, to_server_name)) return;

	auto& cfg = ConfigMgr::Inst();
	if (to_server_name == cfg["SelfServer"]["Name"]) {
		auto to_session = UserMgr::GetInstance()->GetSession(other_id);
		if (to_session) to_session->Send(root.toStyledString(), ID_NOTIFY_WEBRTC_ANSWER_REQ);
		return;
	}

	std::string to_ip_value = "";
	if (!RedisMgr::GetInstance()->Get(USERIPPREFIX + std::to_string(other_id), to_ip_value)) return;

	NotifyVideoEventReq req;
	req.set_from_uid(uid);
	req.set_to_uid(other_id);
	req.set_call_id(call_id);
	req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_WEBRTC_ANSWER);
	req.set_sdp(sdp);

	ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
}

void LogicSystem::WebrtcIceCandidate(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Value root;
	if (!ParseJsonSafe(msg_data, root) || !root.isMember("uid") || !root.isMember("other_id") || !root.isMember("call_id") ||
		!root.isMember("candidate") || !root.isMember("sdpMid") || !root.isMember("sdpMLineIndex")) {
		return;
	}

	int uid = root["uid"].asInt();
	int other_id = root["other_id"].asInt();
	std::string call_id = root["call_id"].asString();

	if (uid <= 0 || other_id <= 0 || call_id.empty()) return;

	// ===== 从 Redis 获取会话并校验 =====
	std::string session_str;
	if (!RedisMgr::GetInstance()->Get(CALL_SESSION_PREFIX + call_id, session_str)) return;

	Json::Value session_json;
	ParseJsonSafe(session_str, session_json);
	int caller_id = session_json["caller_id"].asInt();
	int callee_id = session_json["callee_id"].asInt();

	if ((uid != caller_id && uid != callee_id) || (other_id != caller_id && other_id != callee_id)) return;

	std::string to_server_name;
	if (!GetOnlineServerName(other_id, to_server_name)) return;

	auto& cfg = ConfigMgr::Inst();
	if (to_server_name == cfg["SelfServer"]["Name"]) {
		auto to_session = UserMgr::GetInstance()->GetSession(other_id);
		if (to_session) to_session->Send(root.toStyledString(), ID_NOTIFY_WEBRTC_ICE_CANDIDATE_REQ);
		return;
	}

	std::string to_ip_value = "";
	if (!RedisMgr::GetInstance()->Get(USERIPPREFIX + std::to_string(other_id), to_ip_value)) return;

	NotifyVideoEventReq req;
	req.set_from_uid(uid);
	req.set_to_uid(other_id);
	req.set_call_id(call_id);
	req.set_notify_type(message::VideoNotifyType::VIDEO_NOTIFY_WEBRTC_ICE_CANDIDATE);
	req.set_candidate(root["candidate"].asString());
	req.set_sdpmid(root["sdpMid"].asString());
	req.set_sdpmlineindex(root["sdpMLineIndex"].asInt());

	ChatGrpcClient::GetInstance()->NotifyVideoEvent(to_ip_value, req);
}