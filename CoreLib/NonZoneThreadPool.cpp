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
        m_eventQueue.Stop();

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
        Event event;
        while (m_eventQueue.Dequeue(event))
        {
            if (event.type == EventType::Disconnect)
                handler->Disconnect(event.sessionID);
            else
                handler->Process(event.work.get()); // handler에서 비동기 요청은 복사해서 처리.
            event.work.reset();
        }
        sysLogger->LogInfo("non zone thread", "non zone worker stopped", "threadID", ss.str());
    }

    void NonZoneThreadPool::EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv)  {
        if (!m_running.load(std::memory_order_relaxed))
            return;

        Event event;
        event.type = EventType::Work;
        event.work = std::move(pv);
        if (!m_eventQueue.Enqueue(event, Base::Priority::Droppable))
            errorLogger->LogWarn("non zone thread", "work queue full");
    }

    void NonZoneThreadPool::EnqueueDisconnect(uint64_t sessionID) {
        if (!m_running.load(std::memory_order_relaxed))
            return;

        Event event;
        event.type = EventType::Disconnect;
        event.sessionID = sessionID;
        if (!m_eventQueue.Enqueue(event, Base::Priority::Important)) {
            errorLogger->LogError("non zone thread", "disconnect queue overflow", "session", sessionID);
            handler->Disconnect(sessionID);
        }
    }
}



