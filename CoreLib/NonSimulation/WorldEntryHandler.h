#pragma once

#include <cstdint>
#include <memory>
#include "../Interface/IPacketView.h"
#include "../LoggerGlobal.h"

namespace Core {
    class CacheCommandSender;
    class IServerResponseQueue;
    class ISessionAuth;
    class PacketWriter;
    class SessionManager;

    class WorldEntryHandler {
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        ISessionAuth* auth = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        SessionManager* sessionManager = nullptr;

        void Initialize(IServerResponseQueue* responsesValue, PacketWriter* writerValue, ISessionAuth* authValue, const CacheCommandSender* cacheCommandsValue, SessionManager* sessionManagerValue) {
            responses = responsesValue;
            writer = writerValue;
            auth = authValue;
            cacheCommands = cacheCommandsValue;
            sessionManager = sessionManagerValue;
        }
        bool IsReady() const {
            if (responses == nullptr) {
                sysLogger->LogError("WorldEntryHandler", "responses not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("WorldEntryHandler", "writer not initialized");
                return false;
            }
            if (auth == nullptr) {
                sysLogger->LogError("WorldEntryHandler", "auth not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("WorldEntryHandler", "cacheCommands not initialized");
                return false;
            }
            if (sessionManager == nullptr) {
                sysLogger->LogError("WorldEntryHandler", "sessionManager not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
        void ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID);
    public:
        void CheckSession(IPacketView* p);
        void GetCharacterList(IPacketView* p);
        void GetCharacterState(IPacketView* p);
    };
}
