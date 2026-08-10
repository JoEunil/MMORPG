#pragma once

#include "FlushDispatcher.h"
#include "CacheFlush.h"
#include "MessagePool.h"
#include "DBConnectionGame.h"
#include "DBConnectionBazaar.h"
#include "DBConnectionPool.h"
#include "CacheStorage.h"
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "ProfileCache.h"
#include "InmemoryQueue.h"
#include "Handler.h"
#include "CacheTimer.h"
#include "DBWorker.h"
#include "WALManager.h"

#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

class ProfileCacheTest;

namespace Cache {
	class Initializer {
        CacheFlush flush;
        FlushDispatcher dispatcher;
        CacheStorageInventory cache_inventory;
        CacheStorageCurrency cache_currency;
        ProfileCache cache_profile;
        Handler handler;
        MessagePool msgPool;
        InMemoryQueue recvMQ;
        DBConnectionPool<DBConnectionGame> connectionPoolGame;
        DBConnectionPool<DBConnectionBazaar> connectionPoolBazaar;
        DBWorker<DBConnectionGame> dbWorkerGame;
        DBWorker<DBConnectionBazaar> dbWorkerBazaar;
        WALManager walManager;

        // 테스트에서 Load/Unload/Rename까지 직접 두드리기 위한 concrete getter. friend으로만 접근.
        ProfileCache* GetProfileCacheImpl() {
            return &cache_profile;
        }
        friend class ::ProfileCacheTest;
    public:
        ~Initializer() {
            CleanUp();
        }
        void Initialize() {
            connectionPoolGame.Initialize();
            connectionPoolBazaar.Initialize();
            dbWorkerGame.Initialize(&connectionPoolGame, GAME_DB_WORKER_THREADPOOL_SIZE);
            dbWorkerBazaar.Initialize(&connectionPoolBazaar, BAZAAR_DB_WORKER_THREADPOOL_SIZE);
            cache_inventory.Initialize(&dbWorkerGame);
            cache_currency.Initialize(&dbWorkerGame);
            cache_profile.Initialize(&dbWorkerGame);
            CacheTimer::StartThread();
            
            flush.Initialize(&connectionPoolGame, &connectionPoolBazaar, &cache_inventory, &cache_currency);
            flush.SetInvFlushedFn([this](uint64_t characterID, uint64_t flushedLsn) {
                walManager.OnInventoryFlushed(characterID, flushedLsn);
                });
            dispatcher.Initialize(&flush, &cache_inventory, &cache_currency);
            walManager.Initialize(&cache_inventory, &connectionPoolGame);//  flush, disptcher 보다 뒤에

        }
        
        void InjectDependencies(Core::IMessageQueue* sendMQ)
        {
            handler.Initialize(sendMQ, &msgPool, &dbWorkerGame, &dbWorkerBazaar, &cache_inventory, &cache_currency, &cache_profile);
            recvMQ.Initialize(&handler, &msgPool);
            recvMQ.Start();
        }
        
        bool CheckReady() {
            if (!dispatcher.IsReady()) {
                return false;
            }
            if (!flush.IsReady()) {
                return false;
            }
            if (!msgPool.IsReady()) {
                return false;
            }
            if (!recvMQ.IsReady()) {
                return false;
            }
            if (!cache_inventory.IsReady()) {
                return false;
            }
            if (!cache_currency.IsReady()) {
                return false;
            }
            if (!cache_profile.IsReady()) {
                return false;
            }
            if (!handler.IsReady()) {
                return false;
            }
            if (!dbWorkerGame.IsReady()) {
                return false;
            }
            if (!dbWorkerBazaar.IsReady()) {
                return false;
            }
            if (!walManager.IsReady()) {
                return false;
            }
            return true;
        }
        void CleanUp() {
            CacheTimer::Stop();
            recvMQ.Stop();
            dispatcher.Stop(); // store all dirty data
            flush.Stop();
            dbWorkerGame.Stop();
            dbWorkerBazaar.Stop();
            walManager.Stop();
        }
        Core::IMessageQueue* GetMessageQueue() {
            return static_cast<Core::IMessageQueue*>(&recvMQ);
        }
        Core::IProfileCache* GetProfileCache() {
            return static_cast<Core::IProfileCache*>(&cache_profile);
        }
	};
}
