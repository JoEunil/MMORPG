#include "pch.h"
#include "InMemoryQueue.h"

#include "MessageQueueHandler.h"

namespace Core {
    void InMemoryQueue::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("core mq", "mq thread started", "threadID", ss.str());
        while (true)
        {
            Message* work = nullptr;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_workAvailable.wait(lock, [this] {
                    return !m_running.load(std::memory_order_relaxed) || !m_sharedQueue.empty();
                });

                if (m_sharedQueue.empty())
                    break;

                work = m_sharedQueue.front();
                m_sharedQueue.pop();
            }

            handler->Process(work);
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
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_running.exchange(false, std::memory_order_relaxed))
                return;
        }

        m_workAvailable.notify_all();
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
        coreMsg->SetLength(msg->GetLength());

        bool enqueued = false;
        bool queueFull = false;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_running.load(std::memory_order_relaxed)) {
                if (m_sharedQueue.size() < MQ_SIZE) {
                    m_sharedQueue.push(coreMsg);
                    enqueued = true;
                }
                else {
                    queueFull = true;
                }
            }
        }

        if (!enqueued) {
            if (queueFull)
                errorLogger->LogWarn("core mq", "queue full");
            messagePool->Return(coreMsg);
            return false;
        }

        m_workAvailable.notify_one();
        return true;
    }
}
