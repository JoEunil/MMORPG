#pragma once

#include <thread>
#include <queue>
#include <condition_variable>
#include "NonZoneHandler.h"
#include "IPacketView.h"
#include "Config.h"
#include <BaseLib/LockFreeQueue.h>
#include "LoggerGlobal.h"
namespace Core {
    // 게임틱 단위로 처리되지 않는 (zone 상태와 관련 없는) 요청 처리
    class NonZoneThreadPool {
        std::vector<std::thread> m_threads;
        Base::LockFreeQueue<std::unique_ptr<IPacketView, PacketViewDeleter>, NON_ZONE_QUEUE_SIZE> m_workQueue;
        Base::LockFreeQueue<uint64_t, DISCONNECT_QUEUE_SIZE> m_disconnectQueue;

        std::atomic<bool> m_running = false;
        
        NonZoneHandler* handler;
        void Initialize(NonZoneHandler* h) {
            handler = h;
        }
        void Start();
        void Stop();
        bool IsReady() {
            if (m_threads.size() != NON_ZONE_THREADPOOL_SIZE) {
                sysLogger->LogError("none zone thread", "m_threads not initialized");
                return false;
            }
            if (handler == nullptr) {
                sysLogger->LogError("none zone thread", "handler not initialized");
                return false;
            }
            return true;
        }
        void WorkFunc();
        friend class Initializer;
    public:
        ~NonZoneThreadPool() {
            Stop();
        }
        void EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv);
        void EnqueueDisconnect(uint64_t sessionID);
    };
}
