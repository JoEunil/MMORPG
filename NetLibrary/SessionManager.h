#pragma once
#include <winsock2.h>
#include <cstdint>
#include <cassert>
#include <mutex>
#include <atomic>
#include <new>
#include <unordered_map>
#include <functional>

#include "SessionState.h"
#include "ClientContext.h"
#include "STOverlappedEx.h"


namespace Net{
    // 등록과 해제는 정방향·역방향 샤드를 함께 잠가 일관성을 유지한다.

    struct SessionEntry {
        SessionState state;
        ClientContext* context;
    };

    struct alignas(std::hardware_destructive_interference_size) SessionShard {
        std::mutex mutex;

        std::unordered_map<SOCKET, SessionEntry> sessions;
    };
    struct alignas(std::hardware_destructive_interference_size) ReverseShard {
        std::mutex mutex;

        std::unordered_map<uint64_t, SOCKET> socketMap; // session -> socket 조회용도
    };
    struct PingStruct;
    class ClientContextPool;
    class SessionManager{
        alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> m_connectionCnt = 0;
        alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> m_sessionGenerator = 1;
        char padding[std::hardware_destructive_interference_size - sizeof(uint64_t)];
        std::array<SessionShard, SESSION_SHARD_SIZE> m_sessionShard;
        std::array<ReverseShard, SESSION_SHARD_SIZE> m_reverseShard;
        // AddSession과 Disconnect에 의해서만 추가, 제거가 처리됨.
        // 외부에서 절대 참조할 수 없도록 해야함

        void Initialize(ClientContextPool* c) {
            for (int i = 0; i < SESSION_SHARD_SIZE; i++)
            {
                auto& shard = m_sessionShard[i];
                shard.sessions.reserve(MAX_CLIENT_CONNECTION / SESSION_SHARD_SIZE);
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
            return true;
        }
        ClientContextPool* contextPool = nullptr;
        friend class Initializer;
        friend class NetworkRegressionTest;

    public:
        
        // session 
        bool CheckSession(SOCKET sock) {
            // windows 소켓 핸들은 4의 배수. 
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            if (it == shard.sessions.end())
                return false;
            return it->second.state.CheckSession();
        }

        uint64_t GetSession(SOCKET sock) {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            if (it == shard.sessions.end())
                return 0;
            return it->second.state.GetSessionID();
        }

        // Ping loop에서만 호출: 생존 상태를 복사하고 해당 tick의 미응답 수를 증가한다.
        void PreparePing(int shardID, std::vector<PingStruct>& out);

        void SetContextInvalid(SOCKET sock, uint64_t expectedSessionID) {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            if (it != shard.sessions.end() && it->second.state.GetSessionID() == expectedSessionID)
                it->second.state.SetContextInvalid();
        }
        bool UpdateFlood(SOCKET sock, uint64_t expectedSessionID, uint16_t byte) {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            if (it == shard.sessions.end() || it->second.state.GetSessionID() != expectedSessionID)
                return false;
            // Ping 임계치 판정은 다음 Ping loop가 담당한다. 이번 PONG이
            // 카운트를 초기화할 기회를 수신 경로에서 먼저 차단하지 않는다.
            return it->second.state.BufferReceived(byte);
        }
        ClientContext* GetContext(SOCKET  sock) {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            return it != shard.sessions.end() ? it->second.context : nullptr;
        }

        bool BeginRecvIO(SOCKET sock, uint64_t expectedSessionID, ClientContext*& context,
            uint64_t& sessionID, uint8_t*& buffer, uint16_t& length) {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);

            auto stateIt = shard.sessions.find(sock);
            if (stateIt == shard.sessions.end())
                return false;

            sessionID = stateIt->second.state.GetSessionID();
            if (sessionID != expectedSessionID)
                return false;

            context = stateIt->second.context;
            length = context->BeginRecvIO(sessionID, buffer);
            if (length == 0) {
                context = nullptr;
                return false;
            }
            return true;
        }

        // reverse
        SOCKET GetSocket(uint64_t s) {
            auto& shard = m_reverseShard[s & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.socketMap.find(s);
            return it != shard.socketMap.end() ? it->second : INVALID_SOCKET;
        }

        //session, reverse
        void PongReceived(uint64_t session, uint64_t rtt) {
            SOCKET sock = GetSocket(session);
            if (sock == INVALID_SOCKET)
                return;
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            if (it == shard.sessions.end() || it->second.state.GetSessionID() != session)
                return;
            it->second.state.PongReceived(rtt);
        }
        bool AddSession(SOCKET sock);
        bool Disconnect(SOCKET sock);
        bool Disconnect(SOCKET sock, uint64_t expectedSessionID);
        uint32_t GetConnectionCnt() const {
            return m_connectionCnt.load(std::memory_order_relaxed);
        }

        EnqueueSendResult EnqueueSend(SOCKET sock, STOverlappedEx* overlappedEx) {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            if (it == shard.sessions.end())
                return EnqueueSendResult::Failed;
            return it->second.context->EnqueueSend(overlappedEx);
        }

        STOverlappedEx* DequeueSend(SOCKET sock) {
            auto& shard = m_sessionShard[(sock >> 2) & SESSION_SHARD_MASK];
            std::lock_guard lock(shard.mutex);
            auto it = shard.sessions.find(sock);
            if (it == shard.sessions.end())
                return nullptr;
            return it->second.context->DequeueSend();
        }
    };
}
