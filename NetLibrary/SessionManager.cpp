#include "pch.h"
#include "SessionManager.h"
#include "PingManager.h"
#include "ClientContextPool.h"


namespace Net {
    bool SessionManager::AddSession(SOCKET sock) {
        if (m_connectionCnt.fetch_add(1, std::memory_order_relaxed) >= MAX_CLIENT_CONNECTION) {
            m_connectionCnt.fetch_sub(1, std::memory_order_relaxed);
            return false;
        }
        uint64_t session = 0;
        {
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.contextMap.find(sock);
            if (it != shard.contextMap.end()) {
                // 절대 불가능
                return false;
            }
            session = m_sessionGenerator.fetch_add(1);
            auto ctx = contextPool->Acquire(session);
            if (ctx == nullptr) {
                return false;
            }
            shard.stateMap[sock].SetSession(session);
            shard.contextMap[sock] = ctx;
        }
        {
            auto& shard = m_reverseShard[session & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            shard.socketMap[session] = sock;
        }
        Core::gameLogger->LogInfo("net session", "Session added", "sessionID", session, "socket", sock);
        return true;
    }

    bool SessionManager::Disconnect(SOCKET sock) {
        uint64_t session = 0;
        {
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            {
                auto it = shard.stateMap.find(sock);
                if (it == shard.stateMap.end()) {
                    return false;
                }
                shard.stateMap.erase(it);
            }
            {
                auto it = shard.contextMap.find(sock);
                if (it == shard.contextMap.end()) {
                    return true;
                }
                it->second->Disconnect();
                shard.contextMap.erase(it);
            }
        }
        {
            auto& shard = m_reverseShard[session & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.socketMap.find(session);
            if (it == shard.socketMap.end()) {
                return true;
            }
            shard.socketMap.erase(session);
        }
        m_connectionCnt.fetch_sub(1);
        Core::gameLogger->LogInfo("net session", "Session disconnected", "sessionID", session, "socket", sock);
        return true;
    }

    // spinlock vs mutex
    // - 샤드 단위로 클라이언트가 분산되어, 엔티티 수를 조절할 수 있음.
    // - 샤드로 lock contension 확률이 낮음
    // - 상태 조회 내부에서도 lock이나 루프가 없어서 빠름
    // - unordered_map, out은 단일 접근이기 때문에 때문에 캐시 효음 높음.
    // - 따라서 이 경우 Spin Lock 사용이 효율적임 (mutex보다 context switching 비용 절감)
    void SessionManager::GetSessionSnapshot(int shardID, std::vector<PingStruct>& out) {
        auto& shard = m_sessionShard[shardID & SESSION_SHARD_MASK];
        Base::SpinLockGuard lock(shard.flag);
        out.clear();
        for (auto&[socket, state]: shard.stateMap)
        {
            out.push_back(PingStruct{ socket, state.GetSessionID(), state.GetRtt(), state.CheckSession() });
        }
    }
}