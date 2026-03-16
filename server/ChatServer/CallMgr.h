#pragma once

#include <string>
#include "const.h"
#include <unordered_map>
#include <mutex>
#include "Singleton.h"

struct CallSession
{
    std::string call_id;
    int caller_uid = 0;
    int callee_uid = 0;
    CallStatus status = CALL_INVITING;
    int64_t create_time = 0;
};

class CallMgr
{
public:
    static CallMgr& GetInstance()
    {
        static CallMgr instance;
        return instance;
    }

    std::string CreateCallSession(int caller, int callee)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto now = std::time(nullptr);
        std::string call_id = std::to_string(caller) + "_" + std::to_string(callee) + "_" + std::to_string(now);

        CallSession session;
        session.call_id = call_id;
        session.caller_uid = caller;
        session.callee_uid = callee;
        session.status = CALL_INVITING;
        session.create_time = now;

        _call_sessions[call_id] = session;
        _user_state[caller] = USER_RINGING;
        _user_state[callee] = USER_RINGING;
        return call_id;
    }

    bool IsUserBusy(int uid)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _user_state.find(uid);
        if (it == _user_state.end()) {
            return false;
        }
        return it->second != USER_IDLE;
    }

    bool GetCallSession(const std::string& call_id, CallSession& session)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _call_sessions.find(call_id);
        if (it == _call_sessions.end()) {
            return false;
        }
        session = it->second;
        return true;
    }

    bool AcceptCall(const std::string& call_id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _call_sessions.find(call_id);
        if (it == _call_sessions.end()) {
            return false;
        }
        it->second.status = CALL_ACCEPTED;
        _user_state[it->second.caller_uid] = USER_IN_CALL;
        _user_state[it->second.callee_uid] = USER_IN_CALL;
        return true;
    }

    void EndCall(const std::string& call_id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _call_sessions.find(call_id);
        if (it == _call_sessions.end()) {
            return;
        }

        _user_state[it->second.caller_uid] = USER_IDLE;
        _user_state[it->second.callee_uid] = USER_IDLE;
        _call_sessions.erase(it);
    }

    bool EndCallByUid(int uid)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        for (auto it = _call_sessions.begin(); it != _call_sessions.end(); ++it) {
            const auto& session = it->second;
            if (session.caller_uid == uid || session.callee_uid == uid) {
                _user_state[session.caller_uid] = USER_IDLE;
                _user_state[session.callee_uid] = USER_IDLE;
                _call_sessions.erase(it);
                return true;
            }
        }

        _user_state[uid] = USER_IDLE;
        return false;
    }

    bool GetPeerUidByUid(int uid, int& peer_uid)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        for (auto& kv : _call_sessions) {
            const auto& session = kv.second;
            if (session.caller_uid == uid) {
                peer_uid = session.callee_uid;
                return true;
            }
            if (session.callee_uid == uid) {
                peer_uid = session.caller_uid;
                return true;
            }
        }

        return false;
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, CallSession> _call_sessions;
    std::unordered_map<int, UserCallState> _user_state;
};
