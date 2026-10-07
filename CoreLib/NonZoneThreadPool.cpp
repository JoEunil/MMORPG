#include "pch.h"
#include "NonZoneThreadPool.h"
#include "IPacketView.h"


namespace Core {
    void NonZoneThreadPool::Start() {
        m_running.store(true, std::memory_order_relaxed);

        m_threads.resize(NON_ZONE_THREADPOOL_SIZE);
        for (int i = 0; i < NON_ZONE_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&NonZoneThreadPool::WorkFunc, this);
        }
    }

    void NonZoneThreadPool::Stop() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;

        m_jobSemaphore.release(NON_ZONE_THREADPOOL_SIZE);

        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
        sysLogger->LogInfo("non zone thread", "non zone thread stopped");
    }

    void NonZoneThreadPool::WorkFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("non zone thread", "non zone thread started", "threadID", ss.str());
        while (true)
        {
            m_jobSemaphore.acquire();
            uint64_t session;
            if (m_disconnectQueue.pop(session)) {
                handler->Disconnect(session);
                continue;
            }

            std::unique_ptr<IPacketView, PacketViewDeleter> work;
            if (m_workQueue.pop(work)) {
                if (m_running.load(std::memory_order_relaxed))
                    handler->Process(work.get()); // handler에서 비동기 요청은 복사해서 처리.
                continue;
            }

            if (!m_running.load(std::memory_order_relaxed))
                break;

            errorLogger->LogError("non zone thread", "job semaphore and queues are out of sync");
        }
    }

    void NonZoneThreadPool::EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv)  {
        if (!m_running.load(std::memory_order_relaxed))
            return;

        if(m_workQueue.push(std::move(pv)))
            m_jobSemaphore.release();
    }

    void NonZoneThreadPool::EnqueueDisconnect(uint64_t sessionID) {
        if (!m_running.load(std::memory_order_relaxed))
            return;

        if(m_disconnectQueue.push(sessionID))
            m_jobSemaphore.release();
    }
}



