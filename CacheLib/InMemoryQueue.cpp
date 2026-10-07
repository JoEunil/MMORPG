#include "InMemoryQueue.h"
#include "Handler.h"
#include "MessagePool.h"

namespace Cache {
    void InMemoryQueue::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        Core::sysLogger->LogInfo("cache mq", "mq thread started", "threadID", ss.str());
        while (true) 
        {
			m_workSemaphore.acquire();
            Core::Message* work;
            if (m_sharedQueue.pop(work)) {
                handler->Process(work);
                continue;
            }

            if (!m_running.load(std::memory_order_relaxed))
                break;

            Core::errorLogger->LogError("cache mq", "work semaphore and queue state are out of sync");
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
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
		m_workSemaphore.release(MQ_THREADPOOL_SIZE);
        for (auto& t : m_threads) {
            if (t.joinable())
                t.join();
        }
    }

    bool InMemoryQueue::EnqueueMessage(Core::Message* msg) {
        if (!m_running.load(std::memory_order_relaxed))
            return false;
        auto cacheMsg = messagePool->Acquire();
        if (!cacheMsg) {
            Core::errorLogger->LogError("cache mq", "message pool exhausted");
            return false;
		}
        std::memcpy(cacheMsg->GetBuffer(), msg->GetBuffer(), msg->GetLength());

        if (!m_sharedQueue.push(cacheMsg)) {
            Core::errorLogger->LogWarn("cache mq", "push failed");
            messagePool->Return(cacheMsg);
            return false;
        }
		m_workSemaphore.release();
        return true;
    }
}
