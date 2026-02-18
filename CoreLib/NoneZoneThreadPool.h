#pragma once

#include <thread>
#include <queue>
#include <condition_variable>
#include "NoneZoneHandler.h"
#include "IPacketView.h"
#include "Config.h"
#include <BaseLib/LockFreeQueue.h>
#include <BaseLib/LockFreeQueueUP.h>
#include "LoggerGlobal.h"
namespace Core {
    // 게임틱 단위로 처리되지 않는 (zone 상태와 관련 없는) 요청 처리
    class NoneZoneThreadPool {
        std::vector<std::thread> m_threads;
        Base::LockFreeQueueUP<std::unique_ptr<IPacketView, PacketViewDeleter>, NONE_ZONE_QUEUE_SIZE> m_workQueue;
        Base::LockFreeQueue<uint64_t, DISCONNECT_QUEUE_SIZE> m_disconnectQueue;

        std::atomic<bool> m_running = false;
        
        NoneZoneHandler* handler;
        void Initialize(NoneZoneHandler* h) {
            handler = h;
        }
        void Start();
        void Stop();
        bool IsReady() {
            if (m_threads.size() != NONE_ZONE_THREADPOOL_SIZE) {
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
        ~NoneZoneThreadPool() {
            Stop();
        }
        void EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv);
        void EnqueueDisconnect(uint64_t sessionID);
    };
}
