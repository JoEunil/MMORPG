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
                std::cout << "dispatcher 체크 실패" << std::endl;
                return false;
            }
            if (!flush.IsReady()) {
                std::cout << "flush 체크 실패" << std::endl;
                return false;
            }
            if (!msgPool.IsReady()) {
                std::cout << "msgPool 체크 실패" << std::endl;
                return false;
            }
            if (!recvMQ.IsReady()) {
                std::cout << "recvMQ 체크 실패" << std::endl;
                return false;
            }
            if (!cache_5.IsReady()) {
                std::cout << "cache_5 체크 실패" << std::endl;
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
