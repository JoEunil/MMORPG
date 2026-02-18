#include "pch.h"
#include "NoneZoneThreadPool.h"
#include "IPacketView.h"
#include <iostream>


namespace Core {
    void NoneZoneThreadPool::Start() {
        m_running.store(true);

        m_threads.resize(NONE_ZONE_THREADPOOL_SIZE);
        for (int i = 0; i < NONE_ZONE_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&NoneZoneThreadPool::WorkFunc, this);
        }
    }

    void NoneZoneThreadPool::Stop() {
        m_running.store(false);
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
        sysLogger->LogInfo("none zone thread", "none zone thread stopped");
    }

    void NoneZoneThreadPool::WorkFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("none zone thread", "none zone thread started", "threadID", ss.str());
        while (m_running.load())
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

    void NoneZoneThreadPool::EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv)  {
        m_workQueue.push(std::move(pv));
    }

    void NoneZoneThreadPool::EnqueueDisconnect(uint64_t sessionID) {
        m_disconnectQueue.push(sessionID);
    }
}



