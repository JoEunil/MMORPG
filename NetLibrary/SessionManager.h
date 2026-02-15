#pragma once
#include <winsock2.h>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>

#include "SessionState.h"
#include "ClientContext.h"

#include <BaseLib/SpinLockGuard.h>

namespace Net{
    // 네트워크 레벨의 공유 자료구조를 담기 때문에 shard를 통해 lock contension을 줄이고
    // spin lock을 적용해 lock으로 인한 context switching을 줄인다.
    // critical section이 매우 짧아야 한다.
    struct SessionShard {
        std::atomic_flag flag;
        std::unordered_map<SOCKET, SessionState> stateMap; // socket -> sessionState
        std::unordered_map<uint64_t, ClientContext*> contextMap; // session -> context
    };
    struct ReverseShard {
        std::atomic_flag flag;
        std::unordered_map<uint64_t, SOCKET> socketMap; // session -> socket 조회용도
    };
    struct PingStruct;
    class ClientContextPool;

    class SessionManager{
        std::atomic<uint64_t> m_connectionCnt = 0;
        std::atomic<uint64_t> m_sessionGenerator = 1;
        std::array<SessionShard, SESSION_SHARD_SIZE> m_sessionShard;
        std::array<ReverseShard, SESSION_SHARD_SIZE> m_reverseShard;
        // AddSession과 Disconnect에 의해서만 추가, 제거가 처리됨.
        // 외부에서 절대 참조할 수 없도록 해야함

        void Initialize(ClientContextPool* c) {
            for (int i = 0; i < SESSION_SHARD_SIZE; i++)
            {
                auto& shard = m_sessionShard[i];
                shard.stateMap.reserve(MAX_CLIENT_CONNECTION / SESSION_SHARD_SIZE);
                shard.contextMap.reserve(MAX_CLIENT_CONNECTION / SESSION_SHARD_SIZE);
            }
            for (int i = 0; i < SESSION_SHARD_SIZE; i++)
            {
                auto& shard = m_reverseShard[i];
                shard.socketMap.reserve(MAX_CLIENT_CONNECTION / SESSION_SHARD_SIZE);
            }
            contextPool = c;
        }

        bool IsReady() {
            if (contextPool == nullptr) {
                Core::sysLogger->LogError("net session", "contextPool not initialized");
                return false;
            }
            if (m_sessionShard.size() != SESSION_SHARD_SIZE) {
                Core::sysLogger->LogError("net session", "contextPool not initialized");
                return false;
            }
            return true;
        }
        ClientContextPool* contextPool;
        friend class Initializer;

    public:
        
        // session 
        bool CheckSession(SOCKET sock) {
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it == shard.stateMap.end())
                return false;
            return it->second.CheckSession();
        }

        uint64_t GetSession(SOCKET sock) {
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it == shard.stateMap.end())
                return 0;
            return it->second.GetSessionID();
        }

        void GetSessionSnapshot(int shardID, std::vector<PingStruct>& out);

        void SetContextInvalid(SOCKET sock) {
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it != shard.stateMap.end())
                it->second.SetContextInvalid();
        }
        void UpdateFlood(SOCKET sock, uint16_t byte) {
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it != shard.stateMap.end())
                it->second.BufferReceived(byte);
        }
        ClientContext* GetContext(SOCKET  sock) {
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.contextMap.find(sock);
            return it != shard.contextMap.end() ? it->second : nullptr;
        }

        // reverse
        SOCKET GetSocket(uint64_t s) {
            auto& shard = m_reverseShard[s & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.socketMap.find(s);
            return it != shard.socketMap.end() ? it->second : INVALID_SOCKET;
        }

        //session, reverse
        void PongReceived(uint64_t session, uint64_t rtt) {
            SOCKET sock = GetSocket(session);
            auto& shard = m_sessionShard[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it == shard.stateMap.end())
                return;
            it->second.PongReceived(rtt);
        }
        bool AddSession(SOCKET sock);
        bool Disconnect(SOCKET sock);
        uint32_t GetConnectionCnt() {
            return m_connectionCnt.load(std::memory_order_relaxed);
        }
    };
}
