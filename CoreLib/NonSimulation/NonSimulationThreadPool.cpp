#include "pch.h"
#include "NonSimulationThreadPool.h"
#include "../Interface/IPacketView.h"


namespace Core {
    void NonSimulationThreadPool::Start() {
        m_running.store(true, std::memory_order_relaxed);

        m_threads.resize(NON_SIMULATION_THREADPOOL_SIZE);
        for (int i = 0; i < NON_SIMULATION_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&NonSimulationThreadPool::WorkFunc, this);
        }
    }

    void NonSimulationThreadPool::Stop() {
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

    void NonSimulationThreadPool::WorkFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("non zone thread", "non zone thread started", "threadID", ss.str());
        Event event;
        while (m_eventQueue.Dequeue(event))
        {
            switch (event.type) {
            case EventType::Work:
                handler->Process(event.work.get()); // handler에서 비동기 요청은 복사해서 처리.
                event.work.reset();
                break;
            case EventType::Disconnect:
                handler->Disconnect(event.sessionID);
                break;
            case EventType::LobbyReturn:
                handler->ReturnToLobby(event.transfer);
                event.transfer.reset();
                break;
            }
        }
        sysLogger->LogInfo("non zone thread", "non zone worker stopped", "threadID", ss.str());
    }

    void NonSimulationThreadPool::EnqueuePacket(std::unique_ptr<IPacketView, PacketViewDeleter> packet) {
        if (!m_running.load(std::memory_order_relaxed))
            return;

        Event event;
        event.type = EventType::Work;
        event.work = std::move(packet);
        if (!m_eventQueue.Enqueue(event, Base::Priority::Droppable))
            errorLogger->LogWarn("non zone thread", "work queue full");
    }

    void NonSimulationThreadPool::EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!m_running.load(std::memory_order_relaxed))
            return; // SessionManager saves pending characters during shutdown.

        Event event;
        event.type = EventType::LobbyReturn;
        event.transfer = transfer;
        if (!m_eventQueue.Enqueue(event, Base::Priority::Important)) {
            errorLogger->LogWarn("non zone thread", "lobby return queue overflow");
            handler->ReturnToLobby(transfer);
        }
    }

    void NonSimulationThreadPool::EnqueueDisconnect(uint64_t sessionID) {
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



