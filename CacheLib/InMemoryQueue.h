#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

#include <BaseLib/LockFreeQueue.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/Message.h>
#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/Config.h>
#include "Config.h"

namespace Cache {
    class Handler;
    class MessagePool;
    class InMemoryQueue :public Core::IMessageQueue {
        std::vector<std::thread> m_threads;
        Base::LockFreeQueue<Core::Message*, MQ_SIZE> m_sharedQueue;

        std::atomic<bool> m_running = false;
        Handler* handler;
        MessagePool* messagePool;
        
        void Initialize(Handler* h, MessagePool* m) {
            handler = h;
            messagePool = m;
        }
        
        bool IsReady() {
            if (!m_running.load(std::memory_order_relaxed)) {
                Core::sysLogger->LogError("cache mq", "not running");
                return false;
            } 
            if (m_threads.size() != MQ_THREADPOOL_SIZE) {
                Core::sysLogger->LogError("cache mq", "invalid thread size");
                return false;
            }
            if (handler == nullptr) {
                Core::sysLogger->LogError("cache mq", "handler not initiailized");
                return false;
            }
            if (messagePool == nullptr) {
                Core::sysLogger->LogError("cache mq", "messagePool not initialized");
                return false;
            }
            return true;
        }

        void Start();
        void Stop();

        void ThreadFunc();
        friend class Initializer;
        
    public:
        ~InMemoryQueue() {
            Stop();
        }
        void EnqueueMessage(Core::Message* msg) override; 
    };
}
