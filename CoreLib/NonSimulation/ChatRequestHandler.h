#pragma once

#include <cstdint>
#include <memory>
#include "../Interface/IPacketView.h"
#include "../LoggerGlobal.h"

namespace Core {
    class ChatThreadPool;
    class SessionManager;

    class ChatRequestHandler {
        SessionManager* sessionManager = nullptr;
        ChatThreadPool* chat = nullptr;

        void Initialize(SessionManager* sessionManagerValue, ChatThreadPool* chatValue) {
            sessionManager = sessionManagerValue;
            chat = chatValue;
        }
        bool IsReady() const {
            if (sessionManager == nullptr) {
                sysLogger->LogError("ChatRequestHandler", "sessionManager not initialized");
                return false;
            }
            if (chat == nullptr) {
                sysLogger->LogError("ChatRequestHandler", "chat not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;

    public:
        // false requests disconnect through the caller's event dispatcher.
        bool Chat(IPacketView* p);
    };
}
