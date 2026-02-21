#pragma once

#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>

#include <BaseLib/LockFreeQueueUP.h>
#include "LoggerGlobal.h"
#include "PacketWriter.h"
#include "Config.h"

namespace Core {
    class IIOCP;
    class IPacket;
    class StateManager;
    class CorePerfCollector;
    class BroadcastThreadPool {
        std::vector<std::thread> m_threads;
        Base::LockFreeQueueUP<std::unique_ptr< std::pair<std::vector<std::shared_ptr<IPacket>>, std::vector<std::shared_ptr<IPacket>>>>, BROADCAST_QUEUE_SIZE> m_workQ;

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
            if (!m_running.load()) {
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

        void ThreadFunc();
        void Start();
        void Stop();

        IIOCP* iocp;
        StateManager* stateManager;
        CorePerfCollector* perfCollector;
        PacketWriter* writer;
        friend class Initializer;
    public:
        ~BroadcastThreadPool() {
            Stop();
        }

        void EnqueueWork(std::vector<std::shared_ptr<IPacket>> headers, std::vector<std::shared_ptr<IPacket>> chunks, uint16_t zoneID);
    };

}
