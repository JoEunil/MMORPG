#pragma once
#include <cstdint>
#include <thread>
#include <atomic>
#include <array>
#include <CoreLib/LoggerGlobal.h>
#include "SessionManager.h"
#include "PacketPool.h"
#include "OverlappedExPool.h"
#include "ClientContextPool.h"
#include "Config.h"

namespace Net {
    class NetPerfCollector {
        std::array<std::atomic<uint64_t>, IOCP_THREADPOOL_SIZE> m_recvCount = { 0 };
        std::atomic<uint64_t> jitter;
        std::thread m_thread;
        std::atomic<bool> m_running;
        void Initialize(SessionManager* s, PacketPool* p, OverlappedExPool* o, ClientContextPool* c) {
            sessionManager = s;
            packetPool = p;
            overlappedPool = o;
            contextPool = c;
        }
        bool IsReady() {
            if (sessionManager == nullptr) {
                Core::sysLogger->LogError("net perf", "sessionManager not initialized");
                return false;
            }
            if (packetPool == nullptr) {
                Core::sysLogger->LogError("net perf", "packetPool not initialized");
                return false;
            }
            if (overlappedPool == nullptr) {
                Core::sysLogger->LogError("net perf", "overlappedPool not initialized");
                return false;
            }
            return true;
        }
        SessionManager* sessionManager;
        PacketPool* packetPool;
        OverlappedExPool* overlappedPool;
        ClientContextPool* contextPool;
        friend class Initializer;
    public:
        void Start() {
            m_thread = std::thread(ThreadFunc);
        }
        void Stop() {
            m_running.store(false, std::memory_order_relaxed);
            if (m_thread.joinable())
                m_thread.join();
            Core::sysLogger->LogInfo("net perf", "Net perf collector thread stopped");
        }
        void ThreadFunc() {
            m_running.store(true, std::memory_order_relaxed);
            auto tid = std::this_thread::get_id();
            Core::sysLogger->LogInfo("net perf", "Net perf collector thread started", "threadID", tid);
            while (m_running.load(std::memory_order_relaxed)) 
            {
                Flush();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        void Flush() {
            auto connection = sessionManager->GetConnectionCnt();
            auto pPool = packetPool->GetPoolSize();
            auto oPool = overlappedPool->GetPoolSize();
            auto cPool = contextPool->GetContextPoolSize();
            auto working = contextPool->GetWorkingCnt();
            auto flush = contextPool->GetFlushQueueSize();
            for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++)
            {
                m_recvCount[i].load(std::memory_order_relaxed);
            }
            jitter.load(std::memory_order_relaxed);
            Core::perfLogger->LogInfo("net perf", "perf log per sec", "connection", connection);
            for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++)
            {
                m_recvCount[i].store(0,std::memory_order_relaxed);
            }
            jitter.store(0, std::memory_order_relaxed);
        }
        
        void AddRecvCnt(int index) {
            m_recvCount[index].fetch_add(1, std::memory_order_relaxed);
        }
        void AddJitterCnt() {
            jitter.fetch_add(1, std::memory_order_relaxed);
        }
    };

}