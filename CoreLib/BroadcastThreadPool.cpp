#include "pch.h"
#include "BroadcastThreadPool.h"
#include "IPacket.h"
#include "ZoneState.h"
#include "IIOCP.h"
#include "StateManager.h"

namespace Core {
    void BroadcastThreadPool::ThreadFunc() {
        std::vector<uint64_t> sessions;
        sessions.reserve(MAX_ZONE_CAPACITY);
        while (m_running.load())
        {
            auto work = m_workQ.pop();
            if (work == nullptr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                // back-off
                continue;
            }
            auto packet = work;
            auto zoneID = work->GetZone();
            auto zone = stateManager->GetZone(zoneID);
            zone->GetSessionSnapshot(sessions);
            for (auto session : sessions)
            {
                iocp->SendData(session, packet);
            }
        }
    }

    void BroadcastThreadPool::Start() {
        m_threads.resize(BROADCAST_THREADPOOL_SIZE);
        m_running.store(true);
        for (int i = 0; i < BROADCAST_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&BroadcastThreadPool::ThreadFunc, this);
        }
    }


    void BroadcastThreadPool::Stop() {
        m_running.store(false);
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
    }

    void BroadcastThreadPool::EnqueueWork(std::shared_ptr<IPacket> packet, uint16_t zoneID) {
        if (m_running.load()) {
            packet->SetZone(zoneID);
            m_workQ.push(packet);
            // 실패 시 drop
        }
    }
}
