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
        sysLogger->LogInfo("non zone thread", "non zone thread started", {{"threadID", ss.str()}});
        NonSimulationEvent event;
        while (m_eventQueue.Dequeue(event))
        {
            handler->Process(event);
        }
        sysLogger->LogInfo("non zone thread", "non zone worker stopped", {{"threadID", ss.str()}});
    }

    void NonSimulationThreadPool::EnqueuePacket(std::unique_ptr<IPacketView, PacketViewDeleter> packet) {
        if (!m_running.load(std::memory_order_relaxed))
            return;

        NonSimulationEvent event = NonSimulationWorkEvent{std::move(packet)};
        if (!m_eventQueue.Enqueue(event, Base::Priority::Droppable))
            errorLogger->LogWarn("non zone thread", "work queue full");
    }

    void NonSimulationThreadPool::EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!m_running.load(std::memory_order_relaxed))
            return; // SessionManager saves pending characters during shutdown.

        NonSimulationEvent event = NonSimulationLobbyReturnEvent{transfer};
        if (!m_eventQueue.Enqueue(event, Base::Priority::Important)) {
            errorLogger->LogWarn("non zone thread", "lobby return queue overflow");
            handler->Process(event);
        }
    }

    void NonSimulationThreadPool::EnqueueDisconnect(uint64_t sessionID) {
        if (!m_running.load(std::memory_order_relaxed))
            return;

        NonSimulationEvent event = NonSimulationDisconnectEvent{sessionID};
        if (!m_eventQueue.Enqueue(event, Base::Priority::Important)) {
            errorLogger->LogError("non zone thread", "disconnect queue overflow", {{"session", sessionID}});
            handler->Process(event);
        }
    }
}



