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
            auto headers = packets->first;
            auto chunks = packets->second;
            std::vector<std::vector<std::shared_ptr<IPacket>>> currChunks;
            currChunks.resize(CELLS_X * CELLS_Y);
            perfCollector->AddBroadcastPopCnt();

            uint64_t zoneID = headers[0]->GetZone();
            ZoneState* zone = stateManager->GetZone(zoneID);
            Base::BufferReader<std::vector<std::vector<uint64_t>>> reader = zone->GetSessionSnaphot();
            auto& vec = *reader.data; 
            size_t sentCount = 0;
            for (int i =0 ;i < CELLS_X*CELLS_Y; i++)
            {
                size_t len = 0;
                uint16_t count = 0;
                currChunks[i].clear();
                for (int j : AOI[i])
                {
                    if (chunks[j] == nullptr)
                        continue;
                    len += chunks[j]->GetLength();
                    count += chunks[j]->GetCount();
                    currChunks[i].push_back(chunks[j]);
                }
                if (count == 0)
                    continue;
                writer->AddChunk(headers[i], len, count);
                sentCount += vec[i].size();
                for (auto session : vec[i])
                {
                    iocp->SendDataChunks(session, headers[i], currChunks[i]);
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

    void BroadcastThreadPool::EnqueueWork(std::vector<std::shared_ptr<IPacket>> headers, std::vector<std::shared_ptr<IPacket>> chunks, uint16_t zoneID) {
        perfCollector->AddBroadcastEnqueueCnt();
        if (m_running.load()) {
            headers[0]->SetZone(zoneID);
            m_workQ.push(std::make_unique<std::pair<std::vector<std::shared_ptr<IPacket>>, std::vector<std::shared_ptr<IPacket>>>>(headers, chunks));
            // 실패 시 drop
        }
    }
}
