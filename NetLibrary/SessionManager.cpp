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
        const uint64_t session = m_sessionGenerator.fetch_add(1, std::memory_order_relaxed);
        {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            auto& reverse = m_reverseShard[session & SESSION_SHARD_MASK];
            std::scoped_lock lock(shard.mutex, reverse.mutex);
            assert(shard.sessions.find(sock) == shard.sessions.end());
            auto ctx = contextPool->Acquire(session);
            if (ctx == nullptr) {
                m_connectionCnt.fetch_sub(1, std::memory_order_relaxed);
                return false;
            }
            SessionEntry entry{ {}, ctx };
            entry.state.InitializeSession(session);
            shard.sessions.emplace(sock, std::move(entry));
            assert(reverse.socketMap.find(session) == reverse.socketMap.end());
            reverse.socketMap[session] = sock;
        }
        Core::gameLogger->LogInfo("net session", "Session added", "sessionID", session, "socket", sock);
        return true;
    }

    bool SessionManager::Disconnect(SOCKET sock) {
        uint64_t sessionID = GetSession(sock);
        return sessionID != 0 && Disconnect(sock, sessionID);
    }

    bool SessionManager::Disconnect(SOCKET sock, uint64_t expectedSessionID) {
        const uint64_t session = expectedSessionID;
        {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            auto& reverse = m_reverseShard[session & SESSION_SHARD_MASK];
            std::scoped_lock lock(shard.mutex, reverse.mutex);
            auto stateIt = shard.sessions.find(sock);
            if (stateIt == shard.sessions.end())
                return false;
            if (stateIt->second.state.GetSessionID() != expectedSessionID)
                return false;

            auto reverseIt = reverse.socketMap.find(session);
            assert(reverseIt != reverse.socketMap.end() && reverseIt->second == sock);
            stateIt->second.context->Disconnect();
            contextPool->Return(stateIt->second.context);
            shard.sessions.erase(stateIt);
            reverse.socketMap.erase(reverseIt);
            m_connectionCnt.fetch_sub(1, std::memory_order_relaxed);
        }
        Core::gameLogger->LogInfo("net session", "Session disconnected", "sessionID", session, "socket", sock);
        return true;
    }

    void SessionManager::GetSessionSnapshot(int shardID, std::vector<PingStruct>& out) {
        auto& shard = m_sessionShard[shardID & SESSION_SHARD_MASK];
        std::lock_guard lock(shard.mutex);
        out.clear();
        for (auto&[socket, entry]: shard.sessions)
        {
            auto& state = entry.state;
            const bool alive = state.CheckSession();
            out.emplace_back(socket, state.GetSessionID(), state.GetRtt(), alive);
            if (alive)
                state.PingSent();
        }
    }
}
