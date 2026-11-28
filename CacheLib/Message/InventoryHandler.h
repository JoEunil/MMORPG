#pragma once

#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    class CacheResponseSender;
    class CacheStorageInventory;

    class InventoryHandler {
        CacheResponseSender* responses = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;

        void Initialize(CacheResponseSender* sender, CacheStorageInventory* dependency) {
            responses = sender;
            cache_inventory = dependency;
        }
        bool IsReady() const {
            if (!responses || !cache_inventory) {
                Core::sysLogger->LogError("inventory handler", "dependencies not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    public:
        void Request(uint64_t sessionID, Core::MsgInventoryReqBody request);
        void Update(uint64_t sessionID, Core::MsgInventoryUpdateBody request);
    };
}
