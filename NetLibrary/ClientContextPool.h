#pragma once

#include <deque>
#include <queue>
#include <atomic>
#include <mutex>
#include <memory>
#include <winsock2.h>    

namespace Net {
    class ClientContext;
    class ClientContextPool {
        std::deque<ClientContext*> m_contexts;
        std::queue<ClientContext*> m_tempQ;
        
        std::mutex m_mutex;
        bool m_running = false;
        std::atomic<int> m_workingCnt = 0;
        
        void Initialize();
        bool IsReady() const {
            return m_running;
        }
        void Stop();
        
        void FlushPending();
        void Increase(uint16_t currentSize);
        void Decrease(uint16_t currentSize);
        
        friend class Initializer;
public:
        ClientContext* Acquire(uint64_t session);
        void Return(ClientContext* context);
    };
}
