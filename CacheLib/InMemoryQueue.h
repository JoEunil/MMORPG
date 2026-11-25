#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include <BaseLib/BlockingBackPressureQueue.h>
#include <CoreLib/Interface/IMessageQueue.h>
#include <CoreLib/Interface/IMessage.h>
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/Interface/IDurableLog.h>
#include "Config.h"
#include <CoreLib/Interface/IMessagePool.h>

namespace Cache {
    class Handler;
    class InMemoryQueue :public Core::IMessageQueue {
        std::vector<std::thread> m_threads;
        Base::BlockingBackPressureQueue<Core::ScopedMessage, MQ_SIZE, MQ_DEFER_SIZE, true> m_workQueue;

        std::atomic<bool> m_running = false;
        Handler* handler = nullptr;
        Core::IMessagePool* messagePool = nullptr;
        Core::IDurableLog* durableLogger = nullptr;

        bool WriteDurable(const Core::IMessage* message);
        
        void Initialize(Handler* h, Core::IMessagePool* m, Core::IDurableLog* d) {
            handler = h;
            messagePool = m;
            durableLogger = d;
            m_workQueue.SetDurableLog([this](const Core::ScopedMessage& message) {
                return WriteDurable(message.get());
            });
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
        bool EnqueueMessage(const Core::IMessage* msg) override;
    };
}
