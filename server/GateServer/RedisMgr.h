#pragma once
#include "Singleton.h"
#include "hiredis.h"
#include "RedisConPool.h"
#include "cmath"
#include "ConfigMgr.h"

class RedisMgr : public Singleton<RedisMgr>,
    public std::enable_shared_from_this<RedisMgr>
{
    friend class Singleton<RedisMgr>;
public:
    ~RedisMgr();
    //bool Connect(const std::string& host, int port);
    bool Get(const std::string& key, std::string& value);
    bool Set(const std::string& key, const std::string& value);
    //bool Auth(const std::string& password);
    bool LPush(const std::string& key, const std::string& value);
    bool LPop(const std::string& key, std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    bool RPop(const std::string& key, std::string& value);
    bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
    bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
    std::string HGet(const std::string& key, const std::string& hkey);
    bool Del(const std::string& key);
    bool ExistsKey(const std::string& key);
    void Close();

    std::string acquireLock(const std::string& lockName,
        int lockTimeout, int acquireTimeout);
    bool releaseLock(const std::string& lockName,
        const std::string& identifier);

private:
    RedisMgr();

    std::unique_ptr<RedisConPool> _con_pool;
    //redisContext* _connect;
    //redisReply* _reply;
};