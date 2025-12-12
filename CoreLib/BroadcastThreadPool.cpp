#include "pch.h"
#include "BroadcastThreadPool.h"
#include "Ipacket.h"
#include "ZoneState.h"
#include "IIOCP.h"
#include "StateManager.h"
namespace Core {
    void BroadcastThreadPool::ThreadFunc() {
        std::vector<uint64_t> sessions;
        sessions.reserve(MAX_ZONE_CAPACITY);
        while (m_running.load()) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] { return !m_running || !m_sharedQueue.empty(); });
            if (!m_running.load())
                break;
            while (!m_sharedQueue.empty()) {
                auto work = m_sharedQueue.front();
                m_sharedQueue.pop();
                lock.unlock();
                auto packet = work.first;
                auto zoneID = work.second;
                auto zone = stateManager->GetZone(zoneID);
                zone->GetSessionSnapshot(sessions);
                for (auto session : sessions)
                {
                    iocp->SendData(session, packet);
                }
                lock.lock();
            }
        }
    }

    void BroadcastThreadPool::Start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threads.resize(BROADCAST_THREADPOOL_SIZE);
        m_running.store(true);
        for (int i = 0; i < BROADCAST_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&BroadcastThreadPool::ThreadFunc, this);
        }
    }


    void BroadcastThreadPool::Stop() {
        m_running.store(false);
        m_cv.notify_all();
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
    }

    void BroadcastThreadPool::EnqueueWork(std::shared_ptr<IPacket> packet, uint16_t zoneID) {
        if (m_running.load()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sharedQueue.push({ packet, zoneID });
            m_cv.notify_one();
        }
    }

}
