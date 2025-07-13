#include "MysqlDao.h"
#include "ConfigMgr.h"

MysqlDao::MysqlDao()
{
	auto& cfg = ConfigMgr::Inst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& port = cfg["Mysql"]["Port"];
	const auto& pwd = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& user = cfg["Mysql"]["User"];
	pool_.reset(new MySqlPool(host + ":" + port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao() {
	pool_->Close();
}

int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}
		// ׼�����ô洢����
		std::unique_ptr < sql::PreparedStatement > stmt(con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
		// �����������
		stmt->setString(1, name);
		stmt->setString(2, email);
		stmt->setString(3, pwd);

		// ����PreparedStatement��ֱ��֧��ע�����������������Ҫʹ�ûỰ������������������ȡ���������ֵ

		  // ִ�д洢����
		stmt->execute();
		// ����洢���������˻Ự��������������ʽ��ȡ���������ֵ�������������ִ��SELECT��ѯ����ȡ����
	   // ���磬����洢����������һ���Ự����@result���洢������������������ȡ��
		std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
		if (res->next()) {
			int result = res->getInt("result");
			std::cout << "Result: " << result << std::endl;
			pool_->returnConnection(std::move(con));
			return result;
		}
		pool_->returnConnection(std::move(con));
		return -1;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return -1;
	}
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		// ׼����ѯ���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));

		// �󶨲���
		pstmt->setString(1, name);

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		// ���������
		while (res->next()) {
			std::cout << "Check Email: " << res->getString("email") << std::endl;
			if (email != res->getString("email")) {
				pool_->returnConnection(std::move(con));
				return false;
			}
			pool_->returnConnection(std::move(con));
			return true;
		}
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		// ׼����ѯ���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

		// �󶨲���
		pstmt->setString(2, name);
		pstmt->setString(1, newpwd);

		// ִ�и���
		int updateCount = pstmt->executeUpdate();

		std::cout << "Updated rows: " << updateCount << std::endl;
		pool_->returnConnection(std::move(con));
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name); // ��username�滻Ϊ��Ҫ��ѯ���û���

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::string origin_pwd = "";
		// ���������
		while (res->next()) {
			origin_pwd = res->getString("pwd");
			// �����ѯ��������
			std::cout << "Password: " << origin_pwd << std::endl;
			break;
		}

		if (pwd != origin_pwd) {
			return false;
		}
		userInfo.name = name;
		userInfo.email = res->getString("email");
		userInfo.uid = res->getInt("uid");
		userInfo.pwd = origin_pwd;
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::AddFriendApply(const int& from, const int& to,
	const std::string& desc, const std::string& back_name)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(
			con->_con->prepareStatement("INSERT INTO friend_apply (from_uid, to_uid, descs, back_name) "
				"values (?,?,?,?) "
				"ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid, descs = ?, back_name = ?"));
		pstmt->setInt(1, from); // from id
		pstmt->setInt(2, to);
		pstmt->setString(3, desc);
		pstmt->setString(4, back_name);
		pstmt->setString(5, desc);
		pstmt->setString(6, back_name);
		// ִ�и���
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}


	return true;
}

bool MysqlDao::AuthFriendApply(const int& from, const int& to) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE friend_apply SET status = 1 "
			"WHERE from_uid = ? AND to_uid = ?"));
		//������������ʱfrom����֤ʱto
		pstmt->setInt(1, to); // from id
		pstmt->setInt(2, from);
		// ִ�и���
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}


	return true;
}

bool MysqlDao::AddFriend(const int& from, const int& to, std::string back_name,
	std::vector<std::shared_ptr<AddFriendMsg>>& chat_datas) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {

		//��ʼ����
		con->_con->setAutoCommit(false);
		std::string reverse_back;
		std::string apply_desc;

		{
			// 1. ��������ȡ
			std::unique_ptr<sql::PreparedStatement> selStmt(con->_con->prepareStatement(
				"SELECT back_name, descs "
				"FROM friend_apply "
				"WHERE from_uid = ? AND to_uid = ? "
				"FOR UPDATE"
			));
			selStmt->setInt(1, to);
			selStmt->setInt(2, from);

			std::unique_ptr<sql::ResultSet> rsSel(selStmt->executeQuery());

			if (rsSel->next()) {
				reverse_back = rsSel->getString("back_name");
				apply_desc = rsSel->getString("descs");
			}
			else {
				// û�ж�Ӧ�������¼��ֱ�� rollback ������ʧ��
				con->_con->rollback();
				return false;
			}
		}

		{
			// 2. ִ�������ĸ���
			std::unique_ptr<sql::PreparedStatement> updStmt(con->_con->prepareStatement(
				"UPDATE friend_apply "
				"SET status = 1 "
				"WHERE from_uid = ? AND to_uid = ?"
			));

			updStmt->setInt(1, to);
			updStmt->setInt(2, from);

			if (updStmt->executeUpdate() != 1) {
				// �����������ԣ��ع�
				con->_con->rollback();
				return false;
			}
		}

		{
			// 3. ׼����һ��SQL���, ������֤����������
			std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//������������ʱfrom����֤ʱto
			pstmt->setInt(1, from); // from id
			pstmt->setInt(2, to);
			pstmt->setString(3, back_name);
			// ִ�и���
			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				return false;
			}

			//׼���ڶ���SQL��䣬�������뷽��������
			std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//������������ʱfrom����֤ʱto
			pstmt2->setInt(1, to); // from id
			pstmt2->setInt(2, from);
			pstmt2->setString(3, reverse_back);
			// ִ�и���
			int rowAffected2 = pstmt2->executeUpdate();
			if (rowAffected2 < 0) {
				con->_con->rollback();
				return false;
			}
		}



		// 4. ���� chat_thread
		long long threadId = 0;
		{
			std::unique_ptr<sql::PreparedStatement> threadStmt(con->_con->prepareStatement(
				"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());"
			));

			threadStmt->executeUpdate();

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);

			if (rs->next()) {
				threadId = rs->getInt64(1);
			}
			else {
				return false;
			}
		}

		// 5. ���� private_chat
		{
			std::unique_ptr<sql::PreparedStatement> pcStmt(con->_con->prepareStatement(
				"INSERT INTO private_chat(thread_id, user1_id, user2_id) VALUES (?, ?, ?)"
			));

			pcStmt->setInt64(1, threadId);
			pcStmt->setInt(2, from);
			pcStmt->setInt(3, to);
			if (pcStmt->executeUpdate() < 0) return false;
		}

		// 6. ��ѡ�������ʼ��Ϣ�� chat_message
		if (apply_desc.empty() == false)
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content,created_at, updated_at, status) VALUES (?, ?, ?, ?,NOW(),NOW(),?)"
			));

			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, to);
			msgStmt->setInt(3, from);
			msgStmt->setString(4, apply_desc);
			msgStmt->setInt(5, 0);
			if (msgStmt->executeUpdate() < 0) { return false; }

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);
			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(to);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent(apply_desc);
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				std::cout << "addfriend insert message success" << std::endl;
				chat_datas.push_back(tx_data);
			}
			else {
				return false;
			}
		}

		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status) VALUES (?, ?, ?, ?,NOW(),NOW(),?)"
			));

			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, from);
			msgStmt->setInt(3, to);
			msgStmt->setString(4, "We are friends now!");

			msgStmt->setInt(5, 0);

			if (msgStmt->executeUpdate() < 0) { return false; }

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);
			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(from);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent("We are friends now!");
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				chat_datas.push_back(tx_data);
			}
			else {
				return false;
			}
		}

		// �ύ����
		con->_con->commit();
		std::cout << "addfriend insert friends success" << std::endl;

		return true;
	}
	catch (sql::SQLException& e) {
		// ����������󣬻ع�����
		if (con) {
			con->_con->rollback();
		}
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}


	return true;
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
		pstmt->setInt(1, uid); // ��uid�滻Ϊ��Ҫ��ѯ��uid

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// ���������
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->icon = res->getString("icon");
			user_ptr->uid = uid;
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(std::string name)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name); // ��uid�滻Ϊ��Ҫ��ѯ��uid

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// ���������
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->uid = res->getInt("uid");
			user_ptr->icon = res->getString("icon");
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}


bool MysqlDao::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});


	try {
		// ׼��SQL���, ������ʼid���������������б�
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select apply.from_uid, apply.status, user.name, "
			"user.nick, user.sex from friend_apply as apply join user on apply.from_uid = user.uid where apply.to_uid = ? "
			"and apply.id > ? order by apply.id ASC LIMIT ? "));

		pstmt->setInt(1, touid); // ��uid�滻Ϊ��Ҫ��ѯ��uid
		pstmt->setInt(2, begin); // ��ʼid
		pstmt->setInt(3, limit); //ƫ����
		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// ���������
		while (res->next()) {
			auto name = res->getString("name");
			auto uid = res->getInt("from_uid");
			auto status = res->getInt("status");
			auto nick = res->getString("nick");
			auto sex = res->getInt("sex");
			auto apply_ptr = std::make_shared<ApplyInfo>(uid, name, "", "", nick, sex, status);
			applyList.push_back(apply_ptr);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo> >& user_info_list) {

	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});


	try {
		// ׼��SQL���, ������ʼid���������������б�
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select * from friend where self_id = ? "));

		pstmt->setInt(1, self_id); // ��uid�滻Ϊ��Ҫ��ѯ��uid

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// ���������
		while (res->next()) {
			auto friend_id = res->getInt("friend_id");
			auto back = res->getString("back");
			//��һ�β�ѯfriend_id��Ӧ����Ϣ
			auto user_info = GetUser(friend_id);
			if (user_info == nullptr) {
				continue;
			}

			user_info->back = user_info->name;
			user_info_list.push_back(user_info);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}

	return true;
}

// �����������������loadMore, nextLastId
bool MysqlDao::GetUserThreads(
	int64_t userId,
	int64_t lastId,
	int      pageSize,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads,
	bool& loadMore,
	int& nextLastId)
{
	// ��ʼ״̬
	loadMore = false;
	nextLastId = lastId;
	threads.clear();

	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});
	auto& conn = con->_con;

	try {
		// ׼����ҳ��ѯ��CTE + UNION ALL + ORDER + LIMIT N+1
		std::string sql =
			"WITH all_threads AS ( "
			"  SELECT thread_id, 'private' AS type, user1_id, user2_id "
			"    FROM private_chat "
			"   WHERE (user1_id = ? OR user2_id = ?) "
			"     AND thread_id > ? "
			"  UNION ALL "
			"  SELECT thread_id, 'group'   AS type, 0 AS user1_id, 0 AS user2_id "
			"    FROM group_chat_member "
			"   WHERE user_id   = ? "
			"     AND thread_id > ? "
			") "
			"SELECT thread_id, type, user1_id, user2_id "
			"  FROM all_threads "
			" ORDER BY thread_id "
			" LIMIT ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(
			conn->prepareStatement(sql));

		// �󶨲�����? ��Ӧ (userId, userId, lastId, userId, lastId, pageSize+1)
		int idx = 1;
		pstmt->setInt64(idx++, userId);              // private.user1_id
		pstmt->setInt64(idx++, userId);              // private.user2_id
		pstmt->setInt64(idx++, lastId);              // private.thread_id > lastId
		pstmt->setInt64(idx++, userId);              // group.user_id
		pstmt->setInt64(idx++, lastId);              // group.thread_id > lastId
		pstmt->setInt(idx++, pageSize + 1);          // LIMIT pageSize+1

		// ִ��
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		// �Ȱ������ж�����ʱ����
		std::vector<std::shared_ptr<ChatThreadInfo>> tmp;
		while (res->next()) {
			auto cti = std::make_shared<ChatThreadInfo>();
			cti->_thread_id = res->getInt64("thread_id");
			cti->_type = res->getString("type");
			cti->_user1_id = res->getInt64("user1_id");
			cti->_user2_id = res->getInt64("user2_id");
			tmp.push_back(cti);
		}

		// �ж��Ƿ��ȡ��һ��
		if ((int)tmp.size() > pageSize) {
			loadMore = true;
			tmp.pop_back();  // ������ pageSize+1 ��
		}

		// ����������ݣ����� nextLastId Ϊ���һ���� thread_id
		if (!tmp.empty()) {
			nextLastId = tmp.back()->_thread_id;
		}

		// �����������
		threads = std::move(tmp);
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what()
			<< " (MySQL error code: " << e.getErrorCode()
			<< ", SQLState: " << e.getSQLState() << ")\n";
		return false;
	}

	return true;
}

bool MysqlDao::CreatePrivateChat(int user1_id, int user2_id, int& thread_id)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});
	auto& conn = con->_con;
	try {
		// ��������
		conn->setAutoCommit(false);
		// 1. ��ѯ�Ƿ��Ѵ���˽�Ĳ����м���
		int uid1 = std::min(user1_id, user2_id);
		int uid2 = std::max(user1_id, user2_id);
		std::string check_sql =
			"SELECT thread_id FROM private_chat "
			"WHERE (user1_id = ? AND user2_id = ?) "
			"FOR UPDATE;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(check_sql));
		pstmt->setInt64(1, uid1);
		pstmt->setInt64(2, uid2);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		if (res->next()) {
			// ����Ѵ��ڣ����ظ� thread_id
			thread_id = res->getInt("thread_id");
			conn->commit();  // �ύ����
			return true;
		}

		// 2. ���δ�ҵ��������µ� chat_thread �� private_chat ��¼
		// �� chat_thread ������¼�¼
		std::string insert_chat_thread_sql =
			"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());";

		std::unique_ptr<sql::PreparedStatement> pstmt_insert_thread(conn->prepareStatement(insert_chat_thread_sql));
		pstmt_insert_thread->executeUpdate();

		// ��ȡ�²���� thread_id
		std::string get_last_insert_id_sql = "SELECT LAST_INSERT_ID();";
		std::unique_ptr<sql::PreparedStatement> pstmt_last_insert_id(conn->prepareStatement(get_last_insert_id_sql));
		std::unique_ptr<sql::ResultSet> res_last_id(pstmt_last_insert_id->executeQuery());
		res_last_id->next();
		thread_id = res_last_id->getInt(1);

		// 3. �� private_chat ������¼�¼
		std::string insert_private_chat_sql =
			"INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) "
			"VALUES (?, ?, ?, NOW());";


		std::unique_ptr<sql::PreparedStatement> pstmt_insert_private(conn->prepareStatement(insert_private_chat_sql));
		pstmt_insert_private->setInt64(1, thread_id);
		pstmt_insert_private->setInt64(2, uid1);
		pstmt_insert_private->setInt64(3, uid2);
		pstmt_insert_private->executeUpdate();

		// �ύ����
		conn->commit();
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what() << std::endl;
		conn->rollback();
		return false;
	}
	return false;
}