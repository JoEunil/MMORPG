#include "pch.h"
#include "BroadcastThreadPool.h"
#include "IPacket.h"
#include "ZoneState.h"
#include "IIOCP.h"
#include "StateManager.h"
#include "Cell.h"
#include <BaseLib/TripleBufferAdvanced.h>
namespace Core {
    void BroadcastThreadPool::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("broadcast thread", "broadcast thread started", "threadID", ss.str());
        while (m_running.load())
        {
            auto packets = std::move(m_workQ.pop());
            if (!packets) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                // back-off
                continue;
            }
            perfCollector->AddBroadcastPopCnt();
            uint64_t zoneID = 0;
            ZoneState* zone = nullptr;
            for (auto& packet : *packets)
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
                continue;
            Base::BufferReader<std::vector<std::vector<uint64_t>>> reader = zone->GetSessionSnaphot();
            auto& vec = *reader.data; 
            size_t sentCount = 0;
            for (int i =0 ;i < CELLS_X*CELLS_Y; i++)
            {
                sentCount += vec[i].size();
                if ((*packets)[i] == nullptr)
                    continue;
                for (auto session : vec[i])
                {
                    iocp->SendData(session, (*packets)[i]);
                }
            }
            perfCollector->AddBroadcastSendCnt(sentCount);
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
        perfCollector->AddBroadcastEnqueueCnt();
        if (m_running.load()) {
            
            for (auto& packet : packets) {
                if (packet == nullptr)
                    continue;
                packet->SetZone(zoneID);
            }
            m_workQ.push(std::make_unique<std::vector<std::shared_ptr<IPacket>>>(packets));
            // 실패 시 drop
        }
    }
}
