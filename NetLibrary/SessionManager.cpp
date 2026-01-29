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
        uint64_t session;
        {
            auto& shard = m_stateShards[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it != shard.stateMap.end()) {
                return false;
            }
            session = m_sessionGenerator.fetch_add(1, std::memory_order_relaxed);
            shard.stateMap[sock].SetSession(session);
        }
        if (session == 0) {
            return false;
        }
        {
            auto& shard = m_contextShards[session & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.contextMap.find(session);
            if (it != shard.contextMap.end()) {
                // 절대 불가능
                return false;
            }
            auto ctx = contextPool->Acquire(session);
            if (ctx == nullptr) {
                return false;
            }
            shard.contextMap[session] = ctx;
            shard.socketMap[session] = sock;
        }
        return true;
    }

    // spinlock vs mutex
    // - 샤드 단위로 클라이언트가 분산되어, 엔티티 수를 조절할 수 있음.
    // - 샤드로 lock contension 확률이 낮음
    // - 상태 조회 내부에서도 lock이나 루프가 없어서 빠름
    // - unordered_map, out은 단일 접근이기 때문에 때문에 캐시 효음 높음.
    // - 따라서 이 경우 Spin Lock 사용이 효율적임 (mutex보다 context switching 비용 절감)
    void SessionManager::GetSessionSnapshot(int shardID, std::vector<PingStruct>& out) {
        auto& shard = m_stateShards[shardID & SESSION_SHARD_MASK];
        Base::SpinLockGuard lock(shard.flag);
        out.clear();
        for (auto&[socket, state]: shard.stateMap)
        {
            out.push_back(PingStruct{ socket, state.GetSessionID(), state.GetRtt(), state.CheckSession() });
        }
    }
    bool SessionManager::Disconnect(SOCKET sock) {
        uint64_t session = 0;
        {
            auto& shard = m_stateShards[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it == shard.stateMap.end()) {
                return false;
            }
            session = it->second.GetSessionID();
            shard.stateMap.erase(it);
            if (session == 0)
                return false;
        }

        {
            auto& shard = m_contextShards[session & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.contextMap.find(session);
            if (it == shard.contextMap.end()) {
                return true;
                // context는 제거된 상황이지만 socket이 끊기지 않은 상태 (발생할 일 없음)
            }
            contextPool->Return(it->second);
            shard.socketMap.erase(session);
        }
        m_connectionCnt.fetch_sub(1);
        return true;
    }
}