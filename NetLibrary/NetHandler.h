#pragma once

#include <winsock2.h>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <array>

#include "SessionManager.h"
#include "IAbortSocket.h"
#include "ClientContext.h"
#include "Config.h"

namespace Net {
    class NetHandler {
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
            const bool validNetwork = sessionManager->UpdateFlood(sock, sessionID, len);
            const bool validPacket = validNetwork && ctx->CheckGameSession() && ctx->EnqueueRecvQ(sessionID, buf, len);
            if (!validPacket) {
                sessionManager->SetContextInvalid(sock, sessionID);
                Core::sysLogger->LogInfo("net handler", "game session died", "socket", sock);
                abortSocket->AbortSocket(sock, sessionID);
                return;
            }

        }
        bool OnDisConnect(SOCKET sock) const {
            return sessionManager->Disconnect(sock);
        }
        bool OnDisConnect(SOCKET sock, uint64_t expectedSessionID) const {
            return sessionManager->Disconnect(sock, expectedSessionID);
        }
    };
}
