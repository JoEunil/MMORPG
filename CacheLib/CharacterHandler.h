#pragma once

#include "DB/DBConfig.h"
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/Interface/IMessagePool.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    class CacheResponseSender;
    class CacheStorageInventory;
    class ProfileCache;
    class IResultReader;
    template<typename T> class DBWorker;

    class CharacterHandler {
        CacheResponseSender* responses = nullptr;
        DBWorker<DBConnectionGame>* dbWorkerGame = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;
        ProfileCache* cache_profile = nullptr;

        static Core::MsgCharacterListResBody ReadCharacterList(IResultReader* result);

        void Initialize(CacheResponseSender* sender, DBWorker<DBConnectionGame>* worker,
            CacheStorageInventory* inventory, ProfileCache* profile) {
            responses = sender;
            dbWorkerGame = worker;
            cache_inventory = inventory;
            cache_profile = profile;
        }
        bool IsReady() const {
            if (!responses || !dbWorkerGame || !cache_inventory || !cache_profile) {
                Core::sysLogger->LogError("character handler", "dependencies not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    public:
        void List(uint64_t sessionID, Core::MsgCharacterListReqBody request);
        void LoadState(uint64_t sessionID, Core::MsgCharacterStateReqBody request);
        // Preserve the original request bytes for persistence-queue rejection logs.
        void SaveState(Core::ScopedMessage msg);
        void RenameProfile(uint64_t sessionID, Core::MsgProfileRenameBody request);
    };
}
