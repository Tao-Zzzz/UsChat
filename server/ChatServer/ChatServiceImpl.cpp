#include "ChatServiceImpl.h"
#include "UserMgr.h"
#include "CSession.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "utils.h"

ChatServiceImpl::ChatServiceImpl()
{

}

Status ChatServiceImpl::NotifyAddFriend(ServerContext* context, const AddFriendReq* request, AddFriendRsp* reply)
{
	//查找用户是否在本服务器
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_applyuid(request->applyuid());
		reply->set_touid(request->touid());
		});

	//用户不在内存中则直接返回
	if (session == nullptr) {
		return Status::OK;
	}
	
	//在内存中则直接发送通知对方
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["applyuid"] = request->applyuid();
	rtvalue["name"] = request->name();
	rtvalue["desc"] = request->desc();
	rtvalue["icon"] = request->icon();
	rtvalue["sex"] = request->sex();
	rtvalue["nick"] = request->nick();

	std::string return_str = rtvalue.toStyledString();

	session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
	return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext* context, const AuthFriendReq* request,
	AuthFriendRsp* reply) {
	//查找用户是否在本服务器
	auto touid = request->touid();
	auto fromuid = request->fromuid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_fromuid(request->fromuid());
		reply->set_touid(request->touid());
		});

	//用户不在内存中则直接返回
	if (session == nullptr) {
		return Status::OK;
	}

	//在内存中则直接发送通知对方
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = request->fromuid();
	rtvalue["touid"] = request->touid();

	std::string base_key = USER_BASE_INFO + std::to_string(fromuid);
	auto user_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, fromuid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	auto chat_time = getCurrentTimestamp();
	for(auto& msg : request->textmsgs()) {
		Json::Value  chat;
		chat["sender"] = msg.sender_id();
		chat["msg_id"] = msg.msg_id();
		chat["thread_id"] = msg.thread_id();
		chat["unique_id"] = msg.unique_id();
		chat["msg_content"] = msg.msgcontent();
		chat["chat_time"] = chat_time;
		chat["status"] = msg.status();
		rtvalue["chat_datas"].append(chat);
	}

	std::string return_str = rtvalue.toStyledString();

	session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
	return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(::grpc::ServerContext* context,
	const TextChatMsgReq* request, TextChatMsgRsp* reply) {
	//查找用户是否在本服务器
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);
	reply->set_error(ErrorCodes::Success);

	//用户不在内存中则直接返回
	if (session == nullptr) {
		return Status::OK;
	}

	//在内存中则直接发送通知对方
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = request->fromuid();
	rtvalue["touid"] = request->touid();
	rtvalue["thread_id"] = request->thread_id();
	//将聊天数据组织为数组
	Json::Value text_array;
	for (auto& msg : request->textmsgs()) {
		Json::Value element;
		element["content"] = msg.msgcontent();
		element["unique_id"] = msg.unique_id();
		element["message_id"] = msg.msg_id();
		element["chat_time"] = msg.chat_time();
		text_array.append(element);
	}
	rtvalue["chat_datas"] = text_array;

	std::string return_str = rtvalue.toStyledString();

	session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
	return Status::OK;
}

Status ChatServiceImpl::NotifyGroupTextChatMsg(
	::grpc::ServerContext* context,
	const GroupTextChatMsgReq* request,
	GroupTextChatMsgRsp* reply)
{
	std::cout << "received grpc group chat msg from uid: " << request->fromuid() << std::endl;
	reply->set_error(ErrorCodes::Success);
	reply->set_fromuid(request->fromuid());
	reply->set_thread_id(request->thread_id());

	for (auto uid : request->touids()) {
		reply->add_touids(uid);
	}

	for (const auto& msg : request->textmsgs()) {
		auto* m = reply->add_textmsgs();
		m->set_unique_id(msg.unique_id());
		m->set_msg_id(msg.msg_id());
		m->set_msgcontent(msg.msgcontent());
		m->set_chat_time(msg.chat_time());
	}

	// 核心：遍历 touids
	for (auto touid : request->touids()) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (!session) {
			continue;
		}

		Json::Value rtvalue;
		rtvalue["error"] = ErrorCodes::Success;
		rtvalue["fromuid"] = request->fromuid();
		rtvalue["thread_id"] = request->thread_id();
		rtvalue["touid"] = 0;   // ★关键标志

		Json::Value arr;
		for (const auto& msg : request->textmsgs()) {
			Json::Value e;
			e["content"] = msg.msgcontent();
			e["unique_id"] = msg.unique_id();
			e["message_id"] = msg.msg_id();
			e["chat_time"] = msg.chat_time();
			arr.append(e);
		}

		rtvalue["chat_datas"] = arr;
		std::cout << "send chat req to client " << std::endl;
		session->Send(
			rtvalue.toStyledString(), ID_NOTIFY_TEXT_CHAT_MSG_REQ);
	}

	return Status::OK;
}

bool ChatServiceImpl::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
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

Status ChatServiceImpl::NotifyKickUser(::grpc::ServerContext* context, 
	const KickUserReq* request, KickUserRsp* reply)
{
	//查找用户是否在本服务器
	auto uid = request->uid();
	auto session = UserMgr::GetInstance()->GetSession(uid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_uid(request->uid());
		});

	//用户不在内存中则直接返回
	if (session == nullptr) {
		return Status::OK;
	}

	//在内存中则直接发送通知对方
	session->NotifyOffline(uid);
	//清除旧的连接
	_p_server->ClearSession(session->GetSessionId());

	return Status::OK;
}

void ChatServiceImpl::RegisterServer(std::shared_ptr<CServer> pServer)
{
	_p_server = pServer;
}


Status ChatServiceImpl::NotifyChatImgMsg(::grpc::ServerContext* context, const ::message::NotifyChatImgReq* request, ::message::NotifyChatImgRsp* response)
{
	//查找用户是否在本服务器
	auto uid = request->to_uid();
	auto session = UserMgr::GetInstance()->GetSession(uid);

	Defer defer([request, response]() {
		//设置具体的回包信息
		response->set_error(ErrorCodes::Success);
		response->set_message_id(request->message_id());
		});


	//查找用户是否在本服务器
	auto from_uid = request->from_uid();
	auto from_session = UserMgr::GetInstance()->GetSession(from_uid);

	//用户不在内存中则直接返回
	if (from_session == nullptr) {
		//这里只是返回1个状态
		return Status::OK;
	}

	//在内存中则直接发送通知对方
	from_session->NotifySendClientChatImgRecv(request);
	
	
	
	//用户不在内存中则直接返回
	if (session == nullptr) {
		//这里只是返回1个状态
		return Status::OK;
	}

	//在内存中则直接发送通知对方
	session->NotifyChatImgRecv(request);

	//这里只是返回1个状态
	return Status::OK;
}


Status ChatServiceImpl::NotifyVideoEvent(ServerContext* context,
	const NotifyVideoEventReq* request,
	NotifyVideoEventRsp* reply) {
	auto to_uid = request->to_uid();
	auto from_uid = request->from_uid();

	auto session = UserMgr::GetInstance()->GetSession(to_uid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_from_uid(request->from_uid());
		reply->set_to_uid(request->to_uid());
		reply->set_call_id(request->call_id());
		reply->set_notify_type(request->notify_type());
		});

	// 用户不在本服内存中，直接返回 OK
	if (session == nullptr) {
		return Status::OK;
	}

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["call_id"] = request->call_id();

	short notify_msg_id = 0;

	switch (request->notify_type()) {
	case VIDEO_NOTIFY_INVITE:
		notify_msg_id = ID_NOTIFY_VIDEO_INVITE_REQ;
		rtvalue["from_uid"] = from_uid;
		rtvalue["call_type"] = request->call_type();
		if (!request->name().empty()) {
			rtvalue["name"] = request->name();
		}
		if (!request->icon().empty()) {
			rtvalue["icon"] = request->icon();
		}
		if (!request->nick().empty()) {
			rtvalue["nick"] = request->nick();
		}
		break;

	case VIDEO_NOTIFY_ACCEPT:
		notify_msg_id = ID_NOTIFY_VIDEO_ACCEPT_REQ;
		rtvalue["uid"] = from_uid;
		break;

	case VIDEO_NOTIFY_REJECT:
		notify_msg_id = ID_NOTIFY_VIDEO_REJECT_REQ;
		rtvalue["uid"] = from_uid;
		break;

	case VIDEO_NOTIFY_CANCEL:
		notify_msg_id = ID_NOTIFY_VIDEO_CANCEL_REQ;
		rtvalue["uid"] = from_uid;
		break;

	case VIDEO_NOTIFY_HANGUP:
		notify_msg_id = ID_NOTIFY_VIDEO_HANGUP_REQ;
		rtvalue["uid"] = from_uid;
		break;

	case VIDEO_NOTIFY_WEBRTC_OFFER:
		notify_msg_id = ID_NOTIFY_WEBRTC_OFFER_REQ;
		rtvalue["uid"] = from_uid;
		rtvalue["other_id"] = to_uid;
		rtvalue["sdp"] = request->sdp();
		break;

	case VIDEO_NOTIFY_WEBRTC_ANSWER:
		notify_msg_id = ID_NOTIFY_WEBRTC_ANSWER_REQ;
		rtvalue["uid"] = from_uid;
		rtvalue["other_id"] = to_uid;
		rtvalue["sdp"] = request->sdp();
		break;

	case VIDEO_NOTIFY_WEBRTC_ICE_CANDIDATE:
		notify_msg_id = ID_NOTIFY_WEBRTC_ICE_CANDIDATE_REQ;
		rtvalue["uid"] = from_uid;
		rtvalue["other_id"] = to_uid;
		rtvalue["candidate"] = request->candidate();
		rtvalue["sdpMid"] = request->sdpmid();
		rtvalue["sdpMLineIndex"] = request->sdpmlineindex();
		break;

	default:
		reply->set_error(ErrorCodes::Error_Json);
		return Status::OK;
	}

	session->Send(rtvalue.toStyledString(), notify_msg_id);
	return Status::OK;
}