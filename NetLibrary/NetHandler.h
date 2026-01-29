#pragma once

#include <winsock2.h>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <array>
#include "SessionManager.h"
#include "IAbortSocket.h"
#include "ClientContext.h"
#include "Config.h"

namespace Net {
    class NetHandler {
        std::atomic<int> m_connectionCnt = 0;
        SessionManager* sessionManager;
        IAbortSocket* abortSocket;
        bool IsReady() const {
            if (sessionManager == nullptr)
                return false;
            if (abortSocket == nullptr)
                return false;
            return true;
        }
        void Initialize(SessionManager* s, IAbortSocket* a) {
            sessionManager = s;
            abortSocket = a;
        }
        
        friend class Initializer;
    public:
        bool OnAccept(SOCKET sock) const {
            return sessionManager->AddSession(sock);
        }
        void OnRecv(SOCKET sock, uint8_t* buf, uint16_t len) const {
            sessionManager->UpdateFlood(sock, len);
            if (!sessionManager->CheckSession(sock)) {
                abortSocket->AbortSocket(sock);
                return;
            }
            uint64_t session = sessionManager->GetSession(sock);
            if (!sessionManager->BufferReceived(session, buf, len)) {
                sessionManager->SetContextInvalid(sock);
            }

        }
        bool OnDisConnect(SOCKET sock) const {
            return sessionManager->Disconnect(sock);
        }
        uint16_t AllocateBuffer(SOCKET sock, uint8_t*& buf) const {
            uint64_t session = sessionManager->GetSession(sock);
            if (session == 0) {
                std::cout << "Invalid Session\n";
                return 0;
            }
            ClientContext* ctx = sessionManager->GetContext(session);
            if (ctx == nullptr) {
                std::cout << "Invalid ctx\n";
                return 0;
            }
            return ctx->AllocateRecvBuffer(buf);
        }
    };
}
