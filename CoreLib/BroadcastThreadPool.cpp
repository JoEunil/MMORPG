#include "pch.h"
#include "BroadcastThreadPool.h"
#include "IPacket.h"
#include "ZoneState.h"
#include "IIOCP.h"
#include "StateManager.h"
#include "ZoneArea.h"
#include <BaseLib/TripleBufferAdvanced.h>
namespace Core {
    void BroadcastThreadPool::ThreadFunc() {
        while (m_running.load())
        {
            std::vector<std::shared_ptr<IPacket>> packets;
            if (!m_workQ.pop(packets)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                // back-off
                continue;
            }
            uint64_t zoneID = 0;
            ZoneState* zone = nullptr;
            for (auto& packet : packets)
            {
                if (packet != nullptr)
                {
                    zoneID = packet->GetZone();
                    if (zoneID == 0)
                        continue;
                    zone = stateManager->GetZone(zoneID);
                    break;
                }
            }

            if (zone == nullptr)
                return;
            Base::BufferReader<std::vector<std::vector<uint64_t>>> reader = zone->GetSessionSnaphot();
            auto& vec = *reader.data;
            for (int i =0 ;i < CELLS_X*CELLS_Y; i++)
            {
                if (packets[i] == nullptr)
                    continue;
                for (auto session : vec[i])
                {
                    iocp->SendData(session, packets[i]);
                }
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

    void BroadcastThreadPool::EnqueueWork(std::vector<std::shared_ptr<IPacket>> packets, uint16_t zoneID) {
        if (m_running.load()) {
            for (auto& packet : packets) {
                if (packet == nullptr)
                    continue;
                packet->SetZone(zoneID);
            }
            m_workQ.push(packets);
            // 실패 시 drop
        }
    }
}
