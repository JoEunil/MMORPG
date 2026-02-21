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
        void Initialize(SessionManager* s, PacketPool* p, PacketPool* bp, OverlappedExPool* o, ClientContextPool* c) {
            sessionManager = s;
            packetPool = p;
            bigPacketPool = bp;
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
            if (bigPacketPool == nullptr) {
                Core::sysLogger->LogError("net perf", "bigPacketPool not initialized");
                return false;
            }
            if (overlappedPool == nullptr) {
                Core::sysLogger->LogError("net perf", "overlappedPool not initialized");
                return false;
            }
            if (contextPool == nullptr) {
                Core::sysLogger->LogError("net perf", "contextPool not initialized");
                return false;
            }
            return true;
        }
        SessionManager* sessionManager;
        PacketPool* packetPool;
        PacketPool* bigPacketPool;
        OverlappedExPool* overlappedPool;
        ClientContextPool* contextPool;
        friend class Initializer;
    public:
        void Start() {
            m_thread = std::thread(&NetPerfCollector::ThreadFunc, this);
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
            std::stringstream ss;
            ss << tid;
            Core::sysLogger->LogInfo("net perf", "Net perf collector thread started", "threadID", ss.str());
            while (m_running.load(std::memory_order_relaxed)) 
            {
                Flush();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        void Flush() {
            auto connection = sessionManager->GetConnectionCnt();
            auto pPool = packetPool->GetPoolSize();
            auto bPool = bigPacketPool->GetPoolSize();
            auto oPool = overlappedPool->GetPoolSize();
            auto cPool = contextPool->GetContextPoolSize();
            auto flush = contextPool->GetFlushQueueSize();
            auto jit = jitter.load(std::memory_order_relaxed);
            jitter.store(0, std::memory_order_relaxed);
            Core::perfLogger->LogInfo(
                "net perf",
                "perf log per sec",
                "connection", connection,
                "packetPool", pPool,
                "bigPacketPool", bPool,
                "overlappedPool", oPool,
                "contextPool", cPool,
                "flushQueue", flush,
                "jitter", jit
            );
            for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++)
            {
                Core::perfLogger->LogInfo("net perf", "iocp worker log per sec", "index", i, "recv", m_recvCount[i].load(std::memory_order_relaxed));
                m_recvCount[i].store(0, std::memory_order_relaxed);
            }
            Core::perfLogger->Flush();
        }
        
        void AddRecvCnt(int index) {
            if (index >= IOCP_THREADPOOL_SIZE) {
                Core::errorLogger->LogInfo("net perf", "AddRecvCnt index out of bound", "index", index);
                return;
            }
            m_recvCount[index].fetch_add(1, std::memory_order_relaxed);
        }
        void AddJitterCnt() {
            jitter.fetch_add(1, std::memory_order_relaxed);
        }
    };

}