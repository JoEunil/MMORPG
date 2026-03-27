#include "InMemoryQueue.h"
#include "Handler.h"
#include "MessagePool.h"
#include <iostream>

namespace Cache {
    void InMemoryQueue::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        Core::sysLogger->LogInfo("cache mq", "mq thread started", "threadID", ss.str());
        while (m_running.load(std::memory_order_relaxed)) 
        {
            Core::Message* work;
            if (!m_sharedQueue.pop(work)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }
            handler->Process(work);
        }
        Core::sysLogger->LogInfo("cache mq", "mq thread stopped", "threadID", ss.str());
    }

    void InMemoryQueue::Start() {
        m_threads.resize(MQ_THREADPOOL_SIZE);
        m_running.store(true, std::memory_order_relaxed);
        for (int i = 0; i < MQ_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&InMemoryQueue::ThreadFunc, this);
        }
    }


    void InMemoryQueue::Stop() {
        m_running.store(false, std::memory_order_relaxed);
        
        for (auto& t : m_threads) {
            if (t.joinable())
                t.join();
        }
        Core::Message* work;
        while (m_sharedQueue.pop(work)) {
            handler->Process(work);
        }
    }

    void InMemoryQueue::EnqueueMessage(Core::Message* msg) {
        if (!m_running.load(std::memory_order_relaxed))
            return;
        auto cacheMsg = messagePool->Acquire();
        std::memcpy(cacheMsg->GetBuffer(), msg->GetBuffer(), msg->GetLength());

        if (!m_sharedQueue.push(cacheMsg)) {
            Core::errorLogger->LogWarn("cache mq", "push failed");
        }
    }
}
