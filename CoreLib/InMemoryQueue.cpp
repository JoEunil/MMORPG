#include "pch.h"
#include "InMemoryQueue.h"

#include "MessageQueueHandler.h"

namespace Core {
    void InMemoryQueue::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("core mq", "mq thread started", "threadID", ss.str());
        while (m_running.load(std::memory_order_relaxed)) {
            Message* work;
            if (!m_sharedQueue.pop(work)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }
            handler->Process(work);
        }
        sysLogger->LogInfo("core mq", "mq thread stopped", "threadID", ss.str());
    }

    void InMemoryQueue::Start() {
        m_running.store(true);
		m_threads.resize(MQ_THREADPOOL_SIZE);
        for (int i = 0; i < MQ_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&InMemoryQueue::ThreadFunc, this);
        }
    }

    void InMemoryQueue::Stop() {
        m_running.store(false);

        for (auto& t : m_threads) {
            if (t.joinable())
                t.join();
        }
        Message* work;
        while (m_sharedQueue.pop(work)) {
            handler->Process(work);
        }
    }

    void InMemoryQueue::EnqueueMessage(Core::Message* msg) {
        if (!m_running.load())
            return;
        auto coreMsg = messagePool->Acquire();
        std::memcpy(coreMsg->GetBuffer(), msg->GetBuffer(), msg->GetLength());
  
        if (!m_sharedQueue.push(coreMsg)) {
            errorLogger->LogWarn("core mq", "push failed");
        }
    }
}
