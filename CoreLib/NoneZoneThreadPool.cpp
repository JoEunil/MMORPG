#include "pch.h"
#include "NoneZoneThreadPool.h"
#include "ILogger.h"
#include "IPacketView.h"
#include <iostream>


namespace Core {
    void NoneZoneThreadPool::Start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(true);

        m_threads.resize(NONE_ZONE_THREADPOOL_SIZE);
        for (int i = 0; i < NONE_ZONE_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&NoneZoneThreadPool::WorkFunc, this);
        }
    }

    void NoneZoneThreadPool::Stop() {
        m_running.store(false);
        m_cv.notify_all();
        
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
    }

    void NoneZoneThreadPool::WorkFunc() {
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
                auto work = std::move(m_workQueue.front());
                m_workQueue.pop();
                lock.unlock();
                handler->Process(work.get());
                continue;
            }
        }
    }

    void NoneZoneThreadPool::EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workQueue.push(std::move(pv));
        m_cv.notify_one();
    }

    void NoneZoneThreadPool::EnqueueDisconnect(uint64_t sessionID) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_disconnectQueue.push(sessionID);
        m_cv.notify_one();
    }
}



