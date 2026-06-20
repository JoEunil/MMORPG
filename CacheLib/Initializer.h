#pragma once

#include "FlushDispatcher.h"
#include "CacheFlush.h"
#include "MessagePool.h"
#include "DBConnectionGame.h"
#include "DBConnectionBilling.h"
#include "DBConnectionPool.h"
#include "CacheStorage.h"
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "InmemoryQueue.h"
#include "Handler.h"
#include "CacheTimer.h"
#include "DBWorker.h"

#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
	class Initializer {
        FlushDispatcher dispatcher;
        CacheFlush flush;
        CacheStorageInventory cache_inventory;
        CacheStorageCurrency cache_currency; 
        Handler handler;
        MessagePool msgPool;
        InMemoryQueue recvMQ;
        DBConnectionPool<DBConnectionGame> connectionPoolGame;
        DBConnectionPool<DBConnectionBilling> connectionPoolBilling;
        DBWorker<DBConnectionGame> dbWorkerGame;
        DBWorker<DBConnectionBilling> dbWorkerBilling;
    public:
        ~Initializer() {
            CleanUp();
        }
        void Initialize() {
            connectionPoolGame.Initialize();
            connectionPoolBilling.Initialize();
            dbWorkerGame.Initialize(&connectionPoolGame, GAME_DB_WORKER_THREADPOOL_SIZE);
            dbWorkerBilling.Initialize(&connectionPoolBilling, BILLLING_DB_WORKER_THREADPOOL_SIZE);
            cache_inventory.Initialize(&dbWorkerGame);
            cache_currency.Initialize(&dbWorkerGame);
            CacheTimer::StartThread();
            
            flush.Initialize(&connectionPoolGame, &cache_inventory, &cache_currency);
            dispatcher.Initialize(&flush, &cache_inventory, &cache_currency);
        }
        
        void InjectDependencies(Core::IMessageQueue* sendMQ)
        {
            handler.Initialize(sendMQ, &msgPool, &dbWorkerGame, &dbWorkerBilling, &cache_inventory, &cache_currency);
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
            if (!handler.IsReady()) {
                return false;
            }
            if (!dbWorkerGame.IsReady()) {
                return false;
            }
            if (!dbWorkerBilling.IsReady()) {
                return false;
            }
            return true;
        }
        
        void CleanUp() {
            dispatcher.Stop(); // store all dirty data
            flush.Stop();
            CacheTimer::Stop();
        }
        Core::IMessageQueue* GetMessageQueue() {
            return static_cast<Core::IMessageQueue*>(&recvMQ);
        }
	};
}
