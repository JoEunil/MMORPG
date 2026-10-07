#include "pch.h"
#include "InMemoryQueue.h"

#include "MessageQueueHandler.h"

namespace Core {
    void InMemoryQueue::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("core mq", "mq thread started", "threadID", ss.str());
        while (true) {
			m_workSemaphore.acquire();
            Message* work;
            if (m_sharedQueue.pop(work)) {
                handler->Process(work);
                continue;
            }

            if (!m_running.load(std::memory_order_relaxed))
                break;

            errorLogger->LogError("core mq", "work semaphore and queue state are out of sync");
        }
        sysLogger->LogInfo("core mq", "mq thread stopped", "threadID", ss.str());
    }

    void InMemoryQueue::Start() {
        m_running.store(true, std::memory_order_relaxed);
		m_threads.resize(MQ_THREADPOOL_SIZE);
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
        auto coreMsg = messagePool->Acquire();
        if (coreMsg == nullptr) {
            errorLogger->LogWarn("core mq", "message pool empty");
            return false;
		}
        std::memcpy(coreMsg->GetBuffer(), msg->GetBuffer(), msg->GetLength());

        if (!m_sharedQueue.push(coreMsg)) {
            errorLogger->LogWarn("core mq", "push failed");
            messagePool->Return(coreMsg);
            return false;
        } 
		m_workSemaphore.release();
        return true;
    }
}
