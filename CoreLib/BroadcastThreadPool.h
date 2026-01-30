#pragma once

#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>

#include <BaseLib/LockFreeQueueSP.h>
#include "Config.h"

namespace Core {
    class IIOCP;
    class IPacket;
    class StateManager;
    class BroadcastThreadPool {
        std::vector<std::thread> m_threads;
        Base::LockFreeQueueSP<std::shared_ptr<IPacket>, BROADCAST_QUEUE_SIZE> m_workQ;

        std::atomic<bool> m_running = false;
        
        IIOCP* iocp;
        StateManager* stateManager;

        void Initialize(IIOCP* i, StateManager* s) {
            iocp = i;
            stateManager = s;
        }
        bool IsReady() const {
            if (iocp == nullptr)
                return false;
            if (!m_running.load())
                return false;
            if (m_threads.size() != BROADCAST_THREADPOOL_SIZE)
                return false;
            return true;
        }

        void ThreadFunc();
        void Start();
        void Stop();
        
        friend class Initializer;
    public:
        ~BroadcastThreadPool() {
            Stop();
        }

        void EnqueueWork(std::shared_ptr<Core::IPacket> work, uint16_t zoneID);
    };

}
