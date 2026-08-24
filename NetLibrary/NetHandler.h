#pragma once

#include <winsock2.h>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <array>
#include <new>

#include "SessionManager.h"
#include "IAbortSocket.h"
#include "ClientContext.h"
#include "Config.h"

namespace Net {
    class NetHandler {
        alignas(std::hardware_destructive_interference_size) std::atomic<int> m_connectionCnt = 0;
        char padding[std::hardware_destructive_interference_size];

        SessionManager* sessionManager = nullptr;
        IAbortSocket* abortSocket = nullptr;
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
        void OnRecv(ClientContext* ctx, uint64_t sessionID, SOCKET sock, uint8_t* buf, uint16_t len) const {
            if (ctx == nullptr || !ctx->IsSessionAlive(sessionID))
                return;
            sessionManager->UpdateFlood(sock, len);
            if (!ctx->CheckGameSession()) {
                sessionManager->SetContextInvalid(sock);
                Core::sysLogger->LogInfo("net handler", "game session died", "socket", sock);
                abortSocket->AbortSocket(sock);
                return;
            }
            ctx->EnqueueRecvQ(sessionID, buf, len);

        }
        bool OnDisConnect(SOCKET sock) const {
            return sessionManager->Disconnect(sock);
        }
        bool OnDisConnect(SOCKET sock, uint64_t expectedSessionID) const {
            return sessionManager->Disconnect(sock, expectedSessionID);
        }
    };
}
