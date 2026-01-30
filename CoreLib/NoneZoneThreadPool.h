#pragma once

#include <thread>
#include <queue>
#include <condition_variable>
#include "NoneZoneHandler.h"
#include "IPacketView.h"
#include "Config.h"
namespace Core {
    class ILogger;
    // 게임틱 단위로 처리되지 않는 (zone 상태와 관련 없는) 요청 처리
    class NoneZoneThreadPool {
        std::vector<std::thread> m_threads;
        std::queue<std::unique_ptr<IPacketView, PacketViewDeleter>> m_workQueue;
        std::queue<uint64_t> m_disconnectQueue;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::atomic<bool> m_running = false;
        
        NoneZoneHandler* handler;
        ILogger* logger;
        void Initialize(ILogger* l, NoneZoneHandler* h) {
            logger = l;
            handler = h;
        }
        void Start();
        void Stop();
        bool IsReady() {
            if (logger == nullptr)
                return false;
            if (m_threads.size() != NONE_ZONE_THREADPOOL_SIZE)
                return false;
            if (handler == nullptr)
                return false;
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
