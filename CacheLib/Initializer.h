#pragma once

#include "FlushDispatcher.h"
#include "CacheFlush.h"
#include "MessagePool.h"
#include "DBConnectionPool.h"
#include "CacheStorage.h"
#include "InmemoryQueue.h"
#include "Handler.h"
#include <CoreLib/Ilogger.h>
#include <CoreLib/IMessageQueue.h>

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
        
        void InjectDependencies(Core::IMessageQueue* sendMQ, Core::ILogger* logger)
        {
            handler.Initialize(logger, sendMQ, &msgPool, &connectionPool, &cache_5);
            recvMQ.Initialize(&handler, &msgPool);
            recvMQ.Start();
        }
        
        bool CheckReady(Core::ILogger* logger) {
            logger->LogInfo("Cache check ready start");
            if (!dispatcher.IsReady()) {
                logger->LogError("check ready failed, dispatcher");
                return false;
            }
            if (!flush.IsReady()) {
                logger->LogError("check ready failed, flush");
                return false;
            }
            if (!msgPool.IsReady()) {
                logger->LogError("check ready failed, remsgPoolcvMQ");
                return false;
            }
            if (!recvMQ.IsReady()) {
                logger->LogError("check ready failed, recvMQ");
                return false;
            }
            if (!cache_5.IsReady()) {
                logger->LogError("check ready failed, cache_5");
                return false;
            }
            logger->LogWarn("Cache check ready success");
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
