#pragma once

#include <atomic>
#include <array>
#include <memory>
#include <thread>
#include <vector>

#include <BaseLib/BlockingQueue.h>
#include "LoggerGlobal.h"
#include "PacketWriter.h"
#include "Config.h"

namespace Core {
    class IIOCP;
    class IPacket;
    class StateManager;
    class CorePerfCollector;
    class BroadcastThreadPool {
        struct BroadcastWork {
            std::vector<std::shared_ptr<IPacket>> headers;
            std::vector<std::shared_ptr<IPacket>> chunks;
            uint16_t zoneID = 0;
            uint64_t targetSessionID = 0;
        };
        using BroadcastWorkPtr = std::unique_ptr<BroadcastWork>;

        std::vector<std::thread> m_threads;
        using WorkQueue = Base::BlockingQueue<BroadcastWorkPtr, BROADCAST_QUEUE_SIZE, false>;
        // One consumer per queue preserves each Zone's bootstrap/full/delta order.
        std::array<WorkQueue, BROADCAST_THREADPOOL_SIZE> m_workQueues;
        std::atomic<size_t> m_queuedWorkCount = 0; // Shared queue budget, not a per-worker multiplier.

        std::atomic<bool> m_running = false;
        void Initialize(IIOCP* i, StateManager* s, CorePerfCollector* p, PacketWriter* pw) {
            iocp = i;
            stateManager = s;
            perfCollector = p;
            writer = pw;
        }
        bool IsReady() const {
            if (m_threads.size() != BROADCAST_THREADPOOL_SIZE) {
                sysLogger->LogError("broadcast thread", "m_threads not initialized");
                return false;
            }
            if (!m_running.load(std::memory_order_relaxed)) {
                sysLogger->LogError("broadcast thread", "not running");
                return false;
            }
            if (iocp == nullptr) {
                sysLogger->LogError("broadcast thread", "iocp not initialized");
                return false;
            }
            if (perfCollector == nullptr) {
                sysLogger->LogError("broadcast thread", "perfCollector not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("broadcast thread", "writer not initialized");
                return false;
            }
            return true;
        }

        void ThreadFunc(size_t workerIndex);
        bool Enqueue(BroadcastWorkPtr work);
        void Start();
        void Stop();

        IIOCP* iocp = nullptr;
        StateManager* stateManager = nullptr;
        CorePerfCollector* perfCollector = nullptr;
        PacketWriter* writer = nullptr;
        friend class Initializer;
        friend class ZoneBenchmarkAccess;
        friend class NonZoneThreadPoolFixture;
    public:
        ~BroadcastThreadPool() {
            Stop();
        }

        void EnqueueWork(std::vector<std::shared_ptr<IPacket>> headers, std::vector<std::shared_ptr<IPacket>> chunks, uint16_t zoneID);
        bool EnqueueBootstrap(uint64_t sessionID, uint16_t zoneID,
            std::shared_ptr<IPacket> playerHeader, std::shared_ptr<IPacket> playerChunk,
            std::shared_ptr<IPacket> monsterHeader, std::shared_ptr<IPacket> monsterChunk);
    };

}
