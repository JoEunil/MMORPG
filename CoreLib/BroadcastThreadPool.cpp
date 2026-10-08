#include "pch.h"
#include "BroadcastThreadPool.h"
#include "IPacket.h"
#include "ZoneState.h"
#include "IIOCP.h"
#include "StateManager.h"
#include "Cell.h"
#include <BaseLib/TripleBuffer.h>
namespace Core {
    void BroadcastThreadPool::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("broadcast thread", "broadcast thread started", "threadID", ss.str());

        // work마다 재할당하지 않도록 스레드 로컬로 재사용 (내부 vector의 capacity도 유지된다)
        std::vector<std::vector<std::shared_ptr<IPacket>>> currChunks(CELLS_X * CELLS_Y);

        while (true)
        {
            m_workSemaphore.acquire();
            if (!m_running.load(std::memory_order_relaxed))
                break;

            std::unique_ptr< std::pair<std::vector<std::shared_ptr<IPacket>>, std::vector<std::shared_ptr<IPacket>>>> packets;
            while (!m_workQ.pop(packets)) {
                // drain이 필요하지 않아서 종료 시그널 시 즉시 종료.
                if (!m_running.load(std::memory_order_relaxed))
                    break;

                std::this_thread::yield();
            }

            if (!m_running.load(std::memory_order_relaxed))
                break;

            auto& headers = packets->first;
            auto& chunks = packets->second;
            perfCollector->AddBroadcastPopCnt();

            if (headers.empty())
                continue;

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

            // 다음 work가 올 때까지 packet 참조를 붙들지 않도록 즉시 해제.
            // (버퍼 자체는 재사용하고 shared_ptr만 놓는다)
            for (auto& c : currChunks)
                c.clear();
        }
    }

    void BroadcastThreadPool::Start() {
        m_threads.resize(BROADCAST_THREADPOOL_SIZE);
        m_running.store(true, std::memory_order_relaxed);
        for (int i = 0; i < BROADCAST_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&BroadcastThreadPool::ThreadFunc, this);
        }
    }


    void BroadcastThreadPool::Stop() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
		m_workSemaphore.release(BROADCAST_THREADPOOL_SIZE);
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
    }

    void BroadcastThreadPool::EnqueueWork(std::vector<std::shared_ptr<IPacket>> headers, std::vector<std::shared_ptr<IPacket>> chunks, uint16_t zoneID) {
        perfCollector->AddBroadcastEnqueueCnt();
        if (m_running.load(std::memory_order_relaxed)) {
            headers[0]->SetZone(zoneID);
            if (!m_workQ.push(std::make_unique<std::pair<std::vector<std::shared_ptr<IPacket>>, std::vector<std::shared_ptr<IPacket>>>>(headers, chunks))) {
                // push 실패(full) 시 false 반환
                perfCollector->AddBroadcastDropCnt();
            } else {
                m_workSemaphore.release();
            }
        }
    }
}
