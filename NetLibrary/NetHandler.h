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
            if (sessionManager == nullptr) {
                Core::sysLogger->LogError("net handler", "sessionManager not initialized");
                return false;
            }
            if (abortSocket == nullptr) {
                Core::sysLogger->LogError("net handler", "abortSocket not initialized");
                return false;
            }
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
            auto ctx = sessionManager->GetContext(sock);
            if (!ctx->CheckGameSession()) {
                sessionManager->SetContextInvalid(sock);
                abortSocket->AbortSocket(sock);
                return;
            }
            ctx->EnqueueRecvQ(buf, len);

        }
        bool OnDisConnect(SOCKET sock) const {
            return sessionManager->Disconnect(sock);
        }
        uint16_t AllocateBuffer(SOCKET sock, uint8_t*& buf) const {
            ClientContext* ctx = sessionManager->GetContext(sock);
            if (ctx == nullptr) {
                return 0;
            }
            return ctx->AllocateRecvBuffer(buf);
        }
    };
}
