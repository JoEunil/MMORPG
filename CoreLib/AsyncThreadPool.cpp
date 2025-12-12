#include "pch.h"
#include "AsyncThreadPool.h"
#include "ILogger.h"
#include "IPacketView.h"
#include "AsyncHandler.h"
#include <iostream>


namespace Core {
    void AsyncThreadPool::Start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(true);

        m_threads.resize(ASYNC_THREADPOOL_SIZE);
        for (int i = 0; i < ASYNC_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&AsyncThreadPool::WorkFunc, this);
        }
    }

    void AsyncThreadPool::Stop() {
        m_running.store(false);
        m_cv.notify_all();
        
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
    }

    void AsyncThreadPool::WorkFunc() {
        while (m_running.load()) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] { return !m_disconnectQueue.empty() || !m_workQueue.empty() || !m_running.load(); });
            if (!m_disconnectQueue.empty()) {
                auto work = m_disconnectQueue.front();
                m_disconnectQueue.pop();
                lock.unlock();
                handler->Disconnect(work);
                continue; 
            }
            if (!m_workQueue.empty()) {
                auto work = m_workQueue.front();
                m_workQueue.pop();
                lock.unlock();
                handler->Process(work.get());
                continue;
            }
        }
    }

    void AsyncThreadPool::EnqueueWork(std::shared_ptr<IPacketView> pv) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workQueue.push(pv);
        m_cv.notify_one();
    }

    void AsyncThreadPool::EnqueueDisconnect(uint64_t sessionID) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_disconnectQueue.push(sessionID);
        m_cv.notify_one();
    }
}



