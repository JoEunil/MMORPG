#pragma once

#include <winsock2.h>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <array>
#include "Config.h"

namespace Net {
    class ClientContextPool;
    class ClientContext;
    class NetHandler {
        std::unordered_map<SOCKET, std::unique_ptr<ClientContext>> m_socketMap;
        mutable std::shared_mutex m_smutex;
        std::atomic<int> m_connectionCnt = 0;
        ClientContextPool* contextPool;

        bool IsReady() const {
            return contextPool != nullptr;
        }
        void Initialize(ClientContextPool* p) {
            contextPool = p;
        }
        
        friend class Initializer;
    public:
        bool OnAccept(SOCKET sock);
        void OnRecv(SOCKET sock, uint8_t* buf, uint16_t len) const;
        void OnDisConnect(SOCKET sock);
        ClientContext* GetContext(SOCKET sock) const {
            std::shared_lock<std::shared_mutex> lock(m_smutex);
            auto it = m_socketMap.find(sock);
            return it == m_socketMap.end() ? it->second.get() : nullptr;
        }
        uint16_t AllocateBuffer(SOCKET sock, uint8_t*& buff) const;
    };
}
