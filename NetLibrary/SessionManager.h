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
    struct StateShard {
        std::atomic_flag flag;
        std::unordered_map<SOCKET, SessionState> stateMap; // socket -> sessionState
    };
    struct ContextShard {
        std::atomic_flag flag;
        std::unordered_map<uint64_t, SOCKET> socketMap; // session -> socket 조회용도, Context는 Recv시에만 접근하도록
        std::unordered_map<uint64_t, ClientContext*> contextMap; // session -> context
    };
    struct PingStruct;
    class ClientContextPool;

    class SessionManager{
        std::atomic<uint64_t> m_connectionCnt = 0;
        std::atomic<uint64_t> m_sessionGenerator = 1;
        std::array<StateShard, SESSION_SHARD_SIZE> m_stateShards;
        std::array<ContextShard, SESSION_SHARD_SIZE> m_contextShards;
        // AddSession과 Disconnect에 의해서만 추가, 제거가 처리됨.
        // 외부에서 절대 참조할 수 없도록 해야함

        void Initialize(ClientContextPool* c) {
            for (int i = 0; i < SESSION_SHARD_SIZE; i++)
            {
                auto& shard = m_stateShards[i];
                shard.stateMap.reserve(MAX_CLIENT_CONNECTION / SESSION_SHARD_SIZE);
            }
            for (int i = 0; i < SESSION_SHARD_SIZE; i++)
            {
                auto& shard = m_contextShards[i];
                shard.contextMap.reserve(MAX_CLIENT_CONNECTION / SESSION_SHARD_SIZE);
            }
            contextPool = c;
        }

        ClientContextPool* contextPool;
        friend class Initializer;

    public:
        
        // state 
        bool AddSession(SOCKET sock);
        bool CheckSession(SOCKET sock) {
            auto& shard = m_stateShards[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it == shard.stateMap.end())
                return false;
            return it->second.CheckSession();
        }

        uint64_t GetSession(SOCKET sock) {
            auto& shard = m_stateShards[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it == shard.stateMap.end())
                return 0;
            return it->second.GetSessionID();
        }

        void GetSessionSnapshot(int shardID, std::vector<PingStruct>& out);

        void SetContextInvalid(SOCKET sock) {
            auto& shard = m_stateShards[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it != shard.stateMap.end())
                it->second.SetContextInvalid();
        }
        void UpdateFlood(SOCKET sock, uint16_t byte) {
            auto& shard = m_stateShards[sock & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.stateMap.find(sock);
            if (it != shard.stateMap.end())
                it->second.BufferReceived(byte);
        }
        // context
        ClientContext* GetContext(uint64_t s) {
            auto& shard = m_contextShards[s & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.contextMap.find(s);
            return it != shard.contextMap.end() ? it->second : nullptr;
        }
        SOCKET GetSocket(uint64_t s) {
            auto& shard = m_contextShards[s & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.socketMap.find(s);
            return it != shard.socketMap.end() ? it->second : INVALID_SOCKET;
        }

        bool BufferReceived(uint64_t session, uint8_t* buf, uint16_t len) {
            auto& shard = m_contextShards[session & SESSION_SHARD_MASK];
            Base::SpinLockGuard lock(shard.flag);
            auto it = shard.contextMap.find(session);
            if (it == shard.contextMap.end()) {
                return false;
            }
            else {
                return it->second->EnqueueRecvQ(buf, len);
            }
        }

        void PongReceived(uint64_t s, uint64_t rtt) {
            SOCKET sock;
            {
                auto& shard = m_contextShards[s & SESSION_SHARD_MASK];
                Base::SpinLockGuard lock(shard.flag);
                auto it = shard.socketMap.find(s);
                if (it == shard.socketMap.end())
                    return;
                sock = it->second;
                if (sock == INVALID_SOCKET)
                    return;
            }
            {
                auto& shard = m_stateShards[sock & SESSION_SHARD_MASK];
                Base::SpinLockGuard lock(shard.flag);
                auto it = shard.stateMap.find(sock);
                if (it == shard.stateMap.end())
                    return;
                it->second.PongReceived(rtt);
            }
        }
        bool Disconnect(SOCKET sock);
    };
}
