#pragma once
#include <winsock2.h>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>

#include "ClientContext.h"
#include "NetTimer.h"

namespace Net{
    struct ContextShard {
        std::mutex mutex;
        std::unordered_map<uint64_t, ClientContext*> contextMap;
        // lock contension 줄이기 위함
    };
    class LobbyZone;
    class SessionManager{
        // sesionManager 추가, 삭제는 netHandler에서
        std::atomic<uint64_t> m_sessionID = 0;
        std::array<ContextShard, SHARD_SIZE> m_shards; // mutex있어서 vector 사용 불가

        void Initialize() {
            for (int i = 0; i < SHARD_SIZE; i++)
            {
                auto& shard = m_shards[i];
                shard.contextMap.reserve(MAX_CLIENT_CONNECTION / SHARD_SIZE);
            }
        }

        friend class Initializer;

    public:
        uint64_t GetSessionID() {
            return m_sessionID.fetch_add(1)+1;
        }
        
        void SetContext(ClientContext* ctx) {
            auto& shard = m_shards[ctx->GetSessionID() % SHARD_SIZE];
            std::lock_guard<std::mutex> lock(shard.mutex);
            shard.contextMap[ctx->GetSessionID()] = ctx;
        }
        ClientContext* GetContext(uint64_t s) {
            auto& shard = m_shards[s % SHARD_SIZE];
            std::lock_guard<std::mutex> lock(shard.mutex);
            auto it = shard.contextMap.find(s);
            return it != shard.contextMap.end() ? it->second : nullptr;
        }
        SOCKET GetSocket(uint64_t s) {
            auto& shard = m_shards[s % SHARD_SIZE];
            std::lock_guard<std::mutex> lock(shard.mutex);
            auto it = shard.contextMap.find(s);
            return it != shard.contextMap.end() ? it->second->GetSocket() : INVALID_SOCKET;
        }
        void Disconnect(uint64_t s) {
            auto& shard = m_shards[s % SHARD_SIZE];
            std::lock_guard<std::mutex> lock(shard.mutex);
            shard.contextMap.erase(s);

        }
        void ForEachShard(std::function<void(uint64_t, bool)> f) {
            constexpr size_t MAX_SHARD_SESSION = Core::MAX_USER_CAPACITY / SHARD_SIZE;
            std::array<std::pair<uint64_t, ClientContext*>, MAX_SHARD_SESSION> copyList;
            // heap allocation 없이 stack 사용.
            // 단일 스레드 접근이라 캐시 지역성도 좋음
            for (int i = 0; i < SHARD_SIZE; i++) {
                auto& shard = m_shards[i];
                int idx = 0;
                {
                    std::lock_guard<std::mutex> lock(shard.mutex);
                    for (auto& [sessionID, ctx] : shard.contextMap) {
                        if (ctx)
                            copyList[idx++] = { sessionID, ctx };
                    }
                }
                auto now = NetTimer::GetTimeMS();

                for (int j = 0; j < idx; j++) {
                    auto& [sessionID, ctx] = copyList[j];
                    try {
                        if (!ctx) continue;
                        bool res = ctx->NetStatus();
                        if (res) 
                            ctx->SendPing(now);
                        f(sessionID, res);
                    }
                    catch (...) {
                        // SendPing() 호출 흐름에서 다시 SessionMananger로 돌아와 조회하기 때문에(iocp에서 session->socket 변환)
                        // deadlock이 발생해서, copyList 사용해서 lock 하지 않은 상태로 Send 처리.
                        // 하지만 copyList에서 context의 수명을 보장할 수 없어서, dangling 포인터 문제가 발생한다.
                        // 임시로 try - catch 문으로 사용
                        // Send 전용으로 워커 스레드풀을 만들기에는 Send 작업이 비동기IO 기반이라 짧고,
                        // 굳이 Context Switching만 발생한다고 생각해서 
                        // copyList에 socket을 담아서 한번에 처리. (socket은 invalid socket 체크 로직 때문에 안전함)
                        
                        // Ping 로직에서 기존 패킷 처리 경로를 따르다보니 전달자가 너무 많아져서 Dead Lock이 발생할 수 있는 흐름을 놓쳤음
                        // -> Ping 전용 PacketWriter Interface를 만들어서 예외적으로 직통 경로 만들어줘서 처리.
                    }
                }
            }
        }
        void Pong(uint64_t s) {
            auto& shard = m_shards[s % SHARD_SIZE];
            std::lock_guard<std::mutex> lock(shard.mutex);
            shard.contextMap[s]->PongReceived();
        }
    };
}
