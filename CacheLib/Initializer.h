#pragma once

#include "FlushDispatcher.h"
#include "CacheFlush.h"
#include "MessagePool.h"
#include "DBConnectionPool.h"
#include "CacheStorage.h"
#include "InmemoryQueue.h"
#include "Handler.h"
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
	class Initializer {
        FlushDispatcher dispatcher;
        CacheFlush flush;
        DBConnectionPool connectionPool;
        CacheStorage5 cache_5;
        Handler handler;
        MessagePool msgPool;
        InMemoryQueue recvMQ;
        
    public:
        ~Initializer() {
            CleanUp1();
            CleanUp2();
        }
        void Initialize() {
            cache_5.Initialize();
            connectionPool.Initialize();
            msgPool.Initialize();
            
            flush.Initialize(&connectionPool);
            dispatcher.Initialize(&flush, &cache_5);
        }
        
        void InjectDependencies(Core::IMessageQueue* sendMQ)
        {
            handler.Initialize(sendMQ, &msgPool, &connectionPool, &cache_5);
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
            if (!cache_5.IsReady()) {
                return false;
            }
        }
        
        void CleanUp1() {
            recvMQ.FlushQueue();
        }
        void CleanUp2() {
            dispatcher.Stop(); // store all dirty data
            flush.Stop();
        }
        Core::IMessageQueue* GetMessageQueue() {
            return static_cast<Core::IMessageQueue*>(&recvMQ);
        }
	};
}
