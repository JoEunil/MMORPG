#pragma once

#include "FlushDispatcher.h"
#include "CacheFlush.h"
#include "MessagePool.h"
#include "DBConnectionPool.h"
#include "CacheStorage.h"
#include "InmemoryQueue.h"
#include "Handler.h"
#include "CacheTimer.h"
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
	class Initializer {
        FlushDispatcher dispatcher;
        CacheFlush flush;
        CacheStorage5 cache_5;
        Handler handler;
        MessagePool msgPool;
        InMemoryQueue recvMQ;
        DBConnectionPool connectionPool;
        
    public:
        ~Initializer() {
            CleanUp();
        }
        void Initialize() {
            connectionPool.Initialize();
            cache_5.Initialize(&connectionPool);
            msgPool.Initialize();
            CacheTimer::StartThread();
            
            flush.Initialize(&connectionPool, &cache_5);
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
            if (!handler.IsReady()) {
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
