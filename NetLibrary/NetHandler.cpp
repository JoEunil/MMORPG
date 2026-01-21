#include "pch.h"
#include "NetHandler.h"

#include "ClientContext.h"
#include "ClientContextPool.h"
#include "SessionManager.h"
#include "IAbortSocket.h"
#include "Config.h"
#include <iostream>

namespace Net {
    bool NetHandler::OnAccept(SOCKET sock) {
        if (m_connectionCnt.load() >= MAX_CLIENT_CONNECTION) {
            return false;
        }
        std::unique_lock<std::shared_mutex> lock(m_smutex);
        m_socketMap[sock] = contextPool->Acquire(sock, sessionManager->GetSessionID()); 
        sessionManager->SetContext(m_socketMap[sock].get());
        lock.unlock();
        m_connectionCnt.fetch_add(1);
        return true;
    }

    void NetHandler::OnRecv(SOCKET sock, uint8_t*  buf, uint16_t len) const {
        std::shared_lock<std::shared_mutex> lock(m_smutex);
        auto it = m_socketMap.find(sock);
        if (it != m_socketMap.end()) {
            // 1차 패킷 검증
            if (it->second->FloodCheck()) {
                std::cout << "FloodCheck \n";
                abortSocket->AbortSocket(it->second->GetSessionID());
            }
            if (!it->second->CheckGameSession()) {
                std::cout << "CheckGameSession \n";
                abortSocket->AbortSocket(it->second->GetSessionID());
            }
            it->second->EnqueueRecvQ(buf, len);
        }
    }

    bool NetHandler::OnDisConnect(SOCKET sock) {
        std::unique_lock<std::shared_mutex> lock(m_smutex);
        auto it = m_socketMap.find(sock);
        if (it != m_socketMap.end()) {
            sessionManager->Disconnect(it->second->GetSessionID());
            it->second->Disconnect();
            contextPool->Return(std::move(it->second));
            m_socketMap.erase(it);
            m_connectionCnt.fetch_sub(1);
        }
        else
            return false;
        return true;
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
