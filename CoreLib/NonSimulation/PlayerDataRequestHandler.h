#pragma once

#include <cstdint>
#include <memory>
#include "../Interface/IPacketView.h"
#include "../LoggerGlobal.h"

namespace Core {
    class CacheCommandSender;
    class IProfileCache;
    class IServerResponseQueue;
    class PacketWriter;
    class SessionManager;

    class PlayerDataRequestHandler {
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        SessionManager* sessionManager = nullptr;
        IProfileCache* profileCache = nullptr;

        void Initialize(IServerResponseQueue* responsesValue, PacketWriter* writerValue,
            const CacheCommandSender* cacheCommandsValue, SessionManager* sessionManagerValue,
            IProfileCache* profileCacheValue) {
            responses = responsesValue;
            writer = writerValue;
            cacheCommands = cacheCommandsValue;
            sessionManager = sessionManagerValue;
            profileCache = profileCacheValue;
        }
        bool IsReady() const {
            if (responses == nullptr) {
                sysLogger->LogError("PlayerDataRequestHandler", "responses not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("PlayerDataRequestHandler", "writer not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("PlayerDataRequestHandler", "cacheCommands not initialized");
                return false;
            }
            if (sessionManager == nullptr) {
                sysLogger->LogError("PlayerDataRequestHandler", "sessionManager not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;

    public:
        void RequestProfileRename(uint64_t sessionID, uint32_t profileId, const char* name, uint16_t nameLen);
        void GetInventory(IPacketView* p);
        void WalletRequest(IPacketView* p);
        // false requests disconnect through the caller's event dispatcher.
        bool GetProfileBatch(IPacketView* p);
    };
}
