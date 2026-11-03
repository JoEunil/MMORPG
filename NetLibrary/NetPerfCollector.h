#pragma once
#include <cstdint>
#include <thread>
#include <atomic>
#include <array>
#include <new>
#include <CoreLib/LoggerGlobal.h>
#include "SessionManager.h"
#include "PacketPoolManager.h"
#include "OverlappedExPool.h"
#include "ClientContextPool.h"
#include "Config.h"

namespace Net {
    struct alignas(std::hardware_destructive_interference_size) PaddedAtomicU64 {
        std::atomic<uint64_t> v;
    };

    class NetPerfCollector {
        std::array<PaddedAtomicU64, IOCP_THREADPOOL_SIZE> m_recvCount{};
        alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> jitter;
        char padding[std::hardware_destructive_interference_size - sizeof(uint64_t)];
        std::thread m_thread;
        std::atomic<bool> m_running;
        SessionManager* sessionManager = nullptr;
        PacketPoolManager* packetPoolManager = nullptr;
        OverlappedExPool* overlappedPool = nullptr;
        ClientContextPool* contextPool = nullptr;

        void Initialize(SessionManager* s, PacketPoolManager* p, OverlappedExPool* o, ClientContextPool* c) {
            sessionManager = s;
            packetPoolManager = p;
            overlappedPool = o;
            contextPool = c;
        }
        bool IsReady() {
            if (sessionManager == nullptr) {
                Core::sysLogger->LogError("net perf", "sessionManager not initialized");
                return false;
            }
            if (packetPoolManager == nullptr) {
                Core::sysLogger->LogError("net perf", "packetPool not initialized");
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
            auto pPool = packetPoolManager->GetPoolSize(Core::PacketPoolType::Normal);
            auto bPool = packetPoolManager->GetPoolSize(Core::PacketPoolType::Big);
            auto gPool = packetPoolManager->GetPoolSize(Core::PacketPoolType::Giant);
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
                "giantPacketPool", gPool,
                "overlappedPool", oPool,
                "contextPool", cPool,
                "flushQueue", flush,
                "jitter", jit
            );
            for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++)
            {
                Core::perfLogger->LogInfo("net perf", "iocp worker log per sec", "index", i, "recv", m_recvCount[i].v.load(std::memory_order_relaxed));
                m_recvCount[i].v.store(0, std::memory_order_relaxed);
            }
            Core::perfLogger->Flush();
        }
        
        void AddRecvCnt(int index) {
            if (index >= IOCP_THREADPOOL_SIZE) {
                Core::errorLogger->LogInfo("net perf", "AddRecvCnt index out of bound", "index", index);
                return;
            }
            m_recvCount[index].v.fetch_add(1, std::memory_order_relaxed);
        }
        void AddJitterCnt() {
            jitter.fetch_add(1, std::memory_order_relaxed);
        }
    };

}
