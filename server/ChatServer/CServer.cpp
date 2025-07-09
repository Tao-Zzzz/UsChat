#include "CServer.h"
#include <iostream>
#include "AsioIOServicePool.h"
#include "UserMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"

CServer::CServer(boost::asio::io_context& io_context, short port) :_io_context(io_context), _port(port),
_acceptor(io_context, tcp::endpoint(tcp::v4(), port)), _timer(_io_context, std::chrono::seconds(60))
{
	cout << "Server start success, listen on port : " << _port << endl; 

	StartAccept();
}

CServer::~CServer() {
	cout << "Server destruct listen on port : " << _port << endl;
}

void CServer::HandleAccept(shared_ptr<CSession> new_session, const boost::system::error_code& error){
	if (!error) {
		new_session->Start();
		lock_guard<mutex> lock(_mutex);
		_sessions.insert(make_pair(new_session->GetSessionId(), new_session));
	}
	else {
		cout << "session accept failed, error is " << error.what() << endl;
	}

	StartAccept();
}

// �ӳ�����ȡ����һ��iocontext�󴴽�һ��session��ͻ�������
void CServer::StartAccept() {
	auto &io_context = AsioIOServicePool::GetInstance()->GetIOService();
	shared_ptr<CSession> new_session = make_shared<CSession>(io_context, this);
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, placeholders::_1));
}

//����session ��idɾ��session�����Ƴ��û���session�Ĺ���
void CServer::ClearSession(std::string session_id) {

	lock_guard<mutex> lock(_mutex);
	if (_sessions.find(session_id) != _sessions.end()) {
		auto uid = _sessions[session_id]->GetUserId();

		//�Ƴ��û���session�Ĺ���
		UserMgr::GetInstance()->RmvUserSession(uid, session_id);
	}

	_sessions.erase(session_id);

}

// �󶨵�io_context
void CServer::on_timer(const boost::system::error_code& e)
{
	if(e) {
		cout << "timer error: " << e.message() << endl;
		return;
	}


	std::vector<std::shared_ptr<CSession>> _expired_sessions;
	int session_count = 0;
	{
		lock_guard<mutex> lock(_mutex);
		time_t now = time(nullptr);
		session_count = _sessions.size();
		for (auto iter = _sessions.begin(); iter != _sessions.end(); ++iter) {
			auto b_expired = iter->second->isHeartbeatExpired(now);
			if (b_expired) {
				iter->second->Close();
				// �ռ����ڵ�session
				_expired_sessions.push_back(iter->second);
			}
			else {
				session_count++;
			}
		}
	}

	//����session����
	auto& cfg = ConfigMgr::Inst();
	auto selft_name = cfg["SelfServer"]["Name"];
	auto count_str = std::to_string(session_count);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, selft_name, count_str);

	// ������ڵ�session
	for (auto& session : _expired_sessions) {
		session->DeadExceptionSession();
	}

	//��������һ��60s���
	_timer.expires_after(std::chrono::seconds(60));
	_timer.async_wait([this](boost::system::error_code e) {
		on_timer(e);
		});
}

bool CServer::CheckValid(std::string session_id)
{
	lock_guard<mutex> lock(_mutex);
	auto it = _sessions.find(session_id);
	if(it != _sessions.end()) {
		return true;
	}
	return false;
}

void CServer::StartTimer()
{
	auto self = shared_from_this();
	_timer.async_wait([self](boost::system::error_code e) {
		self->on_timer(e);
		});
}

void CServer::StopTimer()
{
	_timer.cancel();
\
}

