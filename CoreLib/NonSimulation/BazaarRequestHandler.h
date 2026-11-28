#pragma once

#include <cstdint>
#include <memory>
#include "../Interface/IPacketView.h"
#include "../LoggerGlobal.h"

namespace Core {
    class CacheCommandSender;
    class IServerResponseQueue;
    class PacketWriter;
    class SessionManager;

    class BazaarRequestHandler {
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        SessionManager* sessionManager = nullptr;

        void Initialize(IServerResponseQueue* responsesValue, PacketWriter* writerValue, const CacheCommandSender* cacheCommandsValue, SessionManager* sessionManagerValue) {
            responses = responsesValue;
            writer = writerValue;
            cacheCommands = cacheCommandsValue;
            sessionManager = sessionManagerValue;
        }
        bool IsReady() const {
            if (responses == nullptr) {
                sysLogger->LogError("BazaarRequestHandler", "responses not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("BazaarRequestHandler", "writer not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("BazaarRequestHandler", "cacheCommands not initialized");
                return false;
            }
            if (sessionManager == nullptr) {
                sysLogger->LogError("BazaarRequestHandler", "sessionManager not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;

    public:
        void BazaarBuy(IPacketView* p);
        void BazaarRegister(IPacketView* p);
        void BazaarOutboxList(IPacketView* p);
        void BazaarCheckOutbox(IPacketView* p);
        void BazaarCancelListing(IPacketView* p);
        void BazaarClaim(IPacketView* p);
        void BazaarMyList(IPacketView* p);
        void BazaarSearch(IPacketView* p);
    };
}
