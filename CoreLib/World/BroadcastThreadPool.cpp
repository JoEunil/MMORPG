#include "pch.h"
#include "BroadcastThreadPool.h"
#include "../CorePerfCollector.h"
#include "../Interface/IPacket.h"
#include "Interface/ISessionSnapshotReader.h"
#include "../Interface/INetworkSender.h"
#include "../Packet/PacketWriter.h"
#include "Cell.h"
namespace Core {
    void BroadcastThreadPool::ThreadFunc(size_t workerIndex) {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("broadcast thread", "broadcast thread started", {{"threadID", ss.str()}});

        // Reuse each cell's chunk capacity across work items.
        std::vector<std::vector<std::shared_ptr<IPacket>>> cellChunks(CELLS_X * CELLS_Y);
        auto& queue = m_workQueues[workerIndex];
        while (true) {
            BroadcastWorkPtr work;
            if (!queue.pop(work))
                break;
            m_queuedWorkCount.fetch_sub(1, std::memory_order_relaxed);
            perfCollector->AddBroadcastPopCnt();

            if (work->targetSessionID != 0)
                SendBootstrap(*work);
            else
                BroadcastSnapshot(*work, cellChunks);
        }
    }

    void BroadcastThreadPool::SendBootstrap(const BroadcastWork& work) const {
        if (work.headers.size() != 2 || work.chunks.size() != 2)
            return;
        for (size_t i = 0; i < work.headers.size(); ++i) {
            if (!work.headers[i] || !work.chunks[i])
                continue;
            std::vector<std::shared_ptr<IPacket>> chunks;
            if (work.chunks[i]->GetLength() != 0)
                chunks.push_back(work.chunks[i]);
            networkSender->SendDataChunks(work.targetSessionID, work.headers[i], chunks);
        }
        perfCollector->AddBroadcastSendCnt(2);
    }

    bool BroadcastThreadPool::PrepareCellPacket(const BroadcastWork& work, int cellIndex, std::vector<std::shared_ptr<IPacket>>& chunks) const {
        size_t length = 0;
        uint16_t count = 0;
        chunks.clear();
        for (int neighbor : AOI[cellIndex]) {
            const auto& chunk = work.chunks[neighbor];
            if (!chunk)
                continue;
            length += chunk->GetLength();
            count += chunk->GetCount();
            chunks.push_back(chunk);
        }
        if (count == 0)
            return false;
        writer->AddChunk(work.headers[cellIndex], length, count);
        return true;
    }

    void BroadcastThreadPool::BroadcastSnapshot(const BroadcastWork& work, std::vector<std::vector<std::shared_ptr<IPacket>>>& cellChunks) const {
        const auto snapshot = sessionSnapshots->ReadSessionSnapshot(work.zoneID);
        if (!snapshot)
            return;
        const auto& recipientsByCell = *snapshot->data;
        size_t sentCount = 0;
        for (int cellIndex = 0; cellIndex < CELLS_X * CELLS_Y; ++cellIndex) {
            if (!work.headers[cellIndex])
                continue;
            auto& chunks = cellChunks[cellIndex];
            if (!PrepareCellPacket(work, cellIndex, chunks))
                continue;
            sentCount += recipientsByCell[cellIndex].size();
            for (auto session : recipientsByCell[cellIndex])
                networkSender->SendDataChunks(session, work.headers[cellIndex], chunks);
        }
        perfCollector->AddBroadcastSendCnt(sentCount);

        // Release packet references before waiting for the next work item.
        for (auto& chunks : cellChunks)
            chunks.clear();
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
        if (queued < BROADCAST_QUEUE_SIZE) {
            auto& queue = m_workQueues[(work->zoneID - 1) % BROADCAST_THREADPOOL_SIZE];
            if (queue.push(std::move(work)))
                return true;
        }
        m_queuedWorkCount.fetch_sub(1, std::memory_order_relaxed);
        perfCollector->AddBroadcastDropCnt();
        return false;
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
        work->headers.reserve(2);
        work->headers.push_back(std::move(playerHeader));
        work->headers.push_back(std::move(monsterHeader));
        work->chunks.reserve(2);
        work->chunks.push_back(std::move(playerChunk));
        work->chunks.push_back(std::move(monsterChunk));
        work->zoneID = zoneID;
        work->targetSessionID = sessionID;
        return Enqueue(std::move(work));
    }
}
