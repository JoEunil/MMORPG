#pragma once

#include <atomic>
#include <mutex>
#include <memory>
#include <winsock2.h>    
#include <cstdint>

#include <CoreLib/LoggerGlobal.h>
#include <BaseLib/RingQueue.h>
#include "Config.h"

namespace Net {
    class ClientContext;
    class ClientContextPool {
        std::vector<ClientContext*> m_contexts; // LIFO
        Base::RingQueue<ClientContext*, NextPowerOf2(MAX_CLIENT_CONNECTION * 3)> m_flushQ; // ring queue, FIFO

        std::mutex m_mutex;
        bool m_running = false;
        std::atomic<int> m_workingCnt = 0;
        
        void Initialize();
        bool IsReady() const {
            if (!m_running) {
                Core::errorLogger->LogError("context pool", "not running");
                return false;
            }
            return true;
        }
        void Stop();
        
        void FlushPending();
        void Increase(uint16_t currentSize);
        void Decrease(uint16_t currentSize);
        
        friend class Initializer;
public:
        uint32_t GetFlushQueueSize() {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_flushQ.size();
        }
        uint32_t GetContextPoolSize() {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_contexts.size();
        }
        ClientContext* Acquire(uint64_t session);
        void Return(ClientContext* context);
    };
}
