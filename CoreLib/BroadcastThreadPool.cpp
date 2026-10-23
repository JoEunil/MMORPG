#include "pch.h"
#include "BroadcastThreadPool.h"
#include "IPacket.h"
#include "ZoneState.h"
#include "IIOCP.h"
#include "StateManager.h"
#include "Cell.h"
#include <BaseLib/TripleBuffer.h>
namespace Core {
    void BroadcastThreadPool::ThreadFunc(size_t workerIndex) {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("broadcast thread", "broadcast thread started", "threadID", ss.str());

        // work마다 재할당하지 않도록 스레드 로컬로 재사용 (내부 vector의 capacity도 유지된다)
        std::vector<std::vector<std::shared_ptr<IPacket>>> currChunks(CELLS_X * CELLS_Y);

        auto& queue = m_workQueues[workerIndex];
        while (true)
        {
            BroadcastWorkPtr packets;
            if (!queue.pop(packets))
                break;
            m_queuedWorkCount.fetch_sub(1, std::memory_order_relaxed);
            auto& headers = packets->headers;
            auto& chunks = packets->chunks;
            perfCollector->AddBroadcastPopCnt();

            if (packets->targetSessionID != 0) {
                if (headers.size() == 2 && chunks.size() == 2) {
                    for (size_t i = 0; i < headers.size(); ++i) {
                        if (!headers[i] || !chunks[i]) continue;
                        std::vector<std::shared_ptr<IPacket>> oneChunk;
                        if (chunks[i]->GetLength() != 0)
                            oneChunk.push_back(chunks[i]);
                        iocp->SendDataChunks(packets->targetSessionID, headers[i], oneChunk);
                    }
                    perfCollector->AddBroadcastSendCnt(2);
                }
                continue;
            }

            const uint16_t zoneID = packets->zoneID;
            ZoneState* zone = stateManager->GetZone(zoneID);
            Base::BufferReader<std::vector<std::vector<uint64_t>>> reader = zone->GetSessionSnaphot();
            auto& vec = *reader.data; 
            size_t sentCount = 0;
            for (int i =0 ;i < CELLS_X*CELLS_Y; i++)
            {
                if (!headers[i])
                    continue;
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
            m_threads[i] = std::thread(&BroadcastThreadPool::ThreadFunc, this, i);
        }
    }


    void BroadcastThreadPool::Stop() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
        for (auto& queue : m_workQueues)
            queue.Stop();
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
    }

    bool BroadcastThreadPool::Enqueue(BroadcastWorkPtr work) {
        perfCollector->AddBroadcastEnqueueCnt();
        if (!m_running.load(std::memory_order_relaxed) || work->zoneID == 0 || work->zoneID > ZONE_COUNT) {
            perfCollector->AddBroadcastDropCnt();
            return false;
        }
        const auto queued = m_queuedWorkCount.fetch_add(1, std::memory_order_relaxed);
        if (queued >= BROADCAST_QUEUE_SIZE) {
            m_queuedWorkCount.fetch_sub(1, std::memory_order_relaxed);
            perfCollector->AddBroadcastDropCnt();
            return false;
        }
        auto& queue = m_workQueues[(work->zoneID - 1) % BROADCAST_THREADPOOL_SIZE];
        if (!queue.push(std::move(work))) {
            m_queuedWorkCount.fetch_sub(1, std::memory_order_relaxed);
            perfCollector->AddBroadcastDropCnt();
            return false;
        }
        return true;
    }

    void BroadcastThreadPool::EnqueueWork(std::vector<std::shared_ptr<IPacket>> headers, std::vector<std::shared_ptr<IPacket>> chunks, uint16_t zoneID) {
        if (headers.size() != CELLS_X * CELLS_Y || chunks.size() != headers.size()) {
            perfCollector->AddBroadcastDropCnt();
            return;
        }
        auto work = std::make_unique<BroadcastWork>();
        work->headers = std::move(headers);
        work->chunks = std::move(chunks);
        work->zoneID = zoneID;
        Enqueue(std::move(work));
    }

    bool BroadcastThreadPool::EnqueueBootstrap(uint64_t sessionID, uint16_t zoneID,
        std::shared_ptr<IPacket> playerHeader, std::shared_ptr<IPacket> playerChunk,
        std::shared_ptr<IPacket> monsterHeader, std::shared_ptr<IPacket> monsterChunk) {
        if (sessionID == 0
            || !playerHeader || !playerChunk || !monsterHeader || !monsterChunk) {
            perfCollector->AddBroadcastDropCnt();
            return false;
        }
        auto work = std::make_unique<BroadcastWork>();
        work->headers = {std::move(playerHeader), std::move(monsterHeader)};
        work->chunks = {std::move(playerChunk), std::move(monsterChunk)};
        work->zoneID = zoneID;
        work->targetSessionID = sessionID;
        return Enqueue(std::move(work));
    }
}
