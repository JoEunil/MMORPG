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
        std::deque<std::unique_ptr<ClientContext>> m_contexts;
        std::queue<std::unique_ptr<ClientContext>> m_tempQ;
        
        std::mutex m_mutex;
        bool m_running = false;
        std::atomic<int> m_workingCnt = 0;
        
        void Initialize();
        bool IsReady() const {
            return m_running;
        }
        void Stop();
        
        void FlushPending();
        void Adjust();
        void Increase(uint16_t currentSize);
        void Decrease(uint16_t currentSize);
        
        friend class Initializer;
public:
        std::unique_ptr<ClientContext> Acquire(SOCKET sock, uint64_t session);
        void Return(std::unique_ptr<ClientContext> context);
    };
}
