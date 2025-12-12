#pragma once
#include <winsock2.h>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "ClientContext.h"

namespace Net{
    class SessionManager{
        // sesionManager 추가, 삭제는 netHandler에서
        std::atomic<uint64_t> m_sessionID = 0;
        std::unordered_map<uint64_t, ClientContext*> m_contextMap;
        std::mutex m_mutex;
        SessionManager() = default;

    public:
        static SessionManager& Instance() {
            static SessionManager instance;
            return instance;
        }
        
        SessionManager(const SessionManager&) = delete;
        SessionManager& operator=(const SessionManager&) = delete;
        SessionManager(SessionManager&&) = delete;
        SessionManager& operator=(SessionManager&&) = delete;

        uint64_t GetSessionID() {
            return m_sessionID.fetch_add(1)+1;
        }
        
        void SetContext(ClientContext* ctx) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_contextMap[ctx->GetSessionID()] = ctx;
        }
        ClientContext* GetContext(uint64_t s) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_contextMap.find(s);
            return it != m_contextMap.end() ? it->second : nullptr;
        }
        SOCKET GetSocket(uint64_t s) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_contextMap.find(s);
            return it != m_contextMap.end() ? it->second->GetSocket() : INVALID_SOCKET;
        }
        void Disconnect(uint64_t s) {
            std::cout << "SesssionManager DIsconnect \n";
            std::lock_guard<std::mutex> lock(m_mutex);
            m_contextMap.erase(s);
        }
    };
}
