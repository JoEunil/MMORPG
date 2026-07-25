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
        while (m_running.load(std::memory_order_relaxed))
        {
            bool empty = true;
            uint64_t session;
            if (m_disconnectQueue.pop(session)) {
                empty = false;
                handler->Disconnect(session);
            }
            auto work = m_workQueue.pop();
            if (work != nullptr)
                handler->Process(work.get()); // handler에서 비동기 요청은 복사해서 처리.
            if (empty)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void NonZoneThreadPool::EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv)  {
        m_workQueue.push(std::move(pv));
    }

    void NonZoneThreadPool::EnqueueDisconnect(uint64_t sessionID) {
        m_disconnectQueue.push(sessionID);
    }
}



