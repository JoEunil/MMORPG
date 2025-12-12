#include "pch.h"
#include "NetHandler.h"

#include "ClientContext.h"
#include "ClientContextPool.h"
#include "SessionManager.h"
#include "Config.h"
#include <iostream>

namespace Net {
    bool NetHandler::OnAccept(SOCKET sock) {
        if (m_connectionCnt.load() >= MAX_CLIENT_CONNECTION) {
            return false;
        }
        std::unique_lock<std::shared_mutex> lock(m_smutex);
        m_socketMap[sock] = contextPool->Acquire(sock, SessionManager::Instance().GetSessionID()); 
        SessionManager::Instance().SetContext(m_socketMap[sock].get());
        lock.unlock();
        m_connectionCnt.fetch_add(1);
        return true;
    }

    void NetHandler::OnRecv(SOCKET sock, uint8_t*  buf, uint16_t len) const {
        std::shared_lock<std::shared_mutex> lock(m_smutex);
        auto it = m_socketMap.find(sock);
        if (it != m_socketMap.end()) {
            it->second->EnqueueRecvQ(buf, len);
        }
    }

    void NetHandler::OnDisConnect(SOCKET sock) {
        std::cout << "On Disconnect " << sock << std::endl;
        std::unique_lock<std::shared_mutex> lock(m_smutex);
        auto it = m_socketMap.find(sock);
        if (it != m_socketMap.end()) {
            SessionManager::Instance().Disconnect(it->second->GetSessionID());
            it->second->Disconnect();
            contextPool->Return(std::move(it->second));
            m_socketMap.erase(it);
            m_connectionCnt.fetch_sub(1);
        }
    }

    uint16_t NetHandler::AllocateBuffer(SOCKET sock, uint8_t*& buf) const {
        std::shared_lock<std::shared_mutex> lock(m_smutex);
        auto it = m_socketMap.find(sock);
        if (it == m_socketMap.end()) {
            return 0;
        }
        return it->second->AllocateRecvBuffer(buf);
    }
}
