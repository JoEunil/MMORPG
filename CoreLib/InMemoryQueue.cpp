#include "pch.h"
#include "InMemoryQueue.h"

#include "MessageQueueHandler.h"

namespace Core {
    void InMemoryQueue::ThreadFunc() {
        while (m_running.load()) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock,[&] {return !m_running || !m_sharedQueue.empty();});
            if (!m_running.load())
                break;
            while (!m_sharedQueue.empty()) {
                auto work = m_sharedQueue.front();
                m_sharedQueue.pop();
                lock.unlock();
                handler->Process(work);
                lock.lock();
            }
        }
    }

    void InMemoryQueue::Start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threads.resize(MQ_THREADPOOL_SIZE);
        m_running.store(true);
        for (int i = 0; i < MQ_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&InMemoryQueue::ThreadFunc, this);
        }
    }

    void InMemoryQueue::Stop() {
        m_running.store(false);
        m_cv.notify_all();

        for (auto& t : m_threads) {
            if (t.joinable())
                t.join();
        }

        while (!m_sharedQueue.empty()) {
            auto work = m_sharedQueue.front();
            m_sharedQueue.pop();
            handler->Process(work);
        }
    }

    void InMemoryQueue::EnqueueMessage(Core::Message* msg) {
        if (!m_running.load())
            return;
        auto coreMsg = messagePool->Acquire();
        std::memcpy(coreMsg->GetBuffer(), msg->GetBuffer(), msg->GetLength());
    
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sharedQueue.push(coreMsg);
        m_cv.notify_one();
    }
}
