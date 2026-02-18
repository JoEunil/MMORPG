#pragma once

#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>

#include <BaseLib/LockFreeQueue.h>
#include "LoggerGlobal.h"
#include "Config.h"

namespace Core {
    class IIOCP;
    class IPacket;
    class StateManager;
    class CorePerfCollector;
    class BroadcastThreadPool {
        std::vector<std::thread> m_threads;
        Base::LockFreeQueue<std::vector<std::shared_ptr<IPacket>>, BROADCAST_QUEUE_SIZE> m_workQ;

        std::atomic<bool> m_running = false;
        void Initialize(IIOCP* i, StateManager* s, CorePerfCollector* p) {
            iocp = i;
            stateManager = s;
            perfCollector = p;
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
            return true;
        }

        void ThreadFunc();
        void Start();
        void Stop();

        IIOCP* iocp;
        StateManager* stateManager;
        CorePerfCollector* perfCollector;
        friend class Initializer;
    public:
        ~BroadcastThreadPool() {
            Stop();
        }

        void EnqueueWork(std::vector<std::shared_ptr<Core::IPacket>> work, uint16_t zoneID);
    };

}
