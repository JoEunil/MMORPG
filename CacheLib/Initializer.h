#pragma once

#include "FlushDispatcher.h"
#include "CacheFlush.h"
#include "Config.h"
#include <CoreLib/MessagePool.h>
#include "DBConnectionGame.h"
#include "DBConnectionBazaar.h"
#include "DBConnectionPool.h"
#include "CacheStorage.h"
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "ProfileCache.h"
#include "InmemoryQueue.h"
#include "Handler.h"
#include "DBWorker.h"
#include "WALManager.h"

#include <CoreLib/IMessageQueue.h>
#include <CoreLib/ICachedClock.h>
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
        CacheMessagePool msgPool;
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
		explicit Initializer(std::chrono::milliseconds walFsyncInterval = DEFAULT_WAL_FSYNC_INTERVAL,
            std::string walPrefix = "redo")
			: walManager(walFsyncInterval, std::move(walPrefix)) {
		}

        ~Initializer() {
            Shutdown();
        }
        void Initialize(Core::ICachedClock* clock) {
            connectionPoolGame.Initialize();
            connectionPoolBazaar.Initialize();
            dbWorkerGame.Initialize(&connectionPoolGame, GAME_DB_WORKER_THREADPOOL_SIZE);
            dbWorkerBazaar.Initialize(&connectionPoolBazaar, BAZAAR_DB_WORKER_THREADPOOL_SIZE);
            cache_inventory.Initialize(&dbWorkerGame, clock);
            cache_currency.Initialize(&dbWorkerGame, clock);
            cache_profile.Initialize(&dbWorkerGame);
            
            flush.Initialize(&connectionPoolGame, &connectionPoolBazaar, &cache_inventory, &cache_currency);
            flush.SetInvFlushedFn([this](uint64_t characterID, uint64_t flushedLsn) {
                walManager.OnInventoryFlushed(characterID, flushedLsn);
                });
            dispatcher.Initialize(&flush, &cache_inventory, &cache_currency, clock);
            walManager.Initialize(&cache_inventory, &connectionPoolGame);//  flush, disptcher 보다 뒤에

        }
        
        void InjectDependencies(Core::IMessageQueue* sendMQ, Core::DurableLogger* durableLogger = nullptr)
        {
            handler.Initialize(sendMQ, &msgPool, &dbWorkerGame, &dbWorkerBazaar, &cache_inventory, &cache_currency, &cache_profile);
            recvMQ.Initialize(&handler, &msgPool, durableLogger);
            recvMQ.Start();
        }
        
        bool CheckReady() {
            if (!dispatcher.IsReady()
                or !flush.IsReady() 
                or !recvMQ.IsReady()
                or !cache_inventory.IsReady()
                or !cache_currency.IsReady()
                or !cache_profile.IsReady()
                or !handler.IsReady()
                or !dbWorkerGame.IsReady()
                or !dbWorkerBazaar.IsReady()
                or !walManager.IsReady()) {
                return false;
            }
            return true;
        }
        void Shutdown() {
            recvMQ.Stop();
            dbWorkerBazaar.Stop();
            dbWorkerGame.Stop();
            // DB callbacks can mutate inventory/currency (e.g. registration rollback).
            // Finish them before taking the final dirty snapshots.
            dispatcher.Stop();
            flush.Stop();
            walManager.Stop();
        }
        Core::IMessageQueue* GetMessageQueue() {
            return static_cast<Core::IMessageQueue*>(&recvMQ);
        }
        Core::IInventoryOperations* GetInventoryOperations() { return &cache_inventory; }
        Core::IProfileCache* GetProfileCache() {
            return static_cast<Core::IProfileCache*>(&cache_profile);
        }
	};
}
