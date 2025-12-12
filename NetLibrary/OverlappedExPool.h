#pragma once
#include <deque>
#include <mutex>
#include <cstdint>
#include <queue>
#include "STOverlappedEx.h"
#include "Config.h"


namespace Net {
    static constexpr size_t ACCEPT_BUFFER_SIZE = (sizeof(SOCKADDR_IN) + 16) * 2;
    class OverlappedExPool {
        std::deque<STOverlappedEx*> m_overlappedPool;

        std::queue<char*> m_acceptBuffers;
        std::mutex m_bufMutex;
        std::mutex m_mutex;
        
        ~OverlappedExPool();
        void Initialize();
        bool IsReady() {
            return m_overlappedPool.size() > 0;
        }
        void Adjust();
        void Increase(uint16_t currentSize);
        void Decrease(uint16_t currentSize);
        
        friend class Initializer;
    public:
        STOverlappedEx* Acquire();
        void Return(STOverlappedEx*);
        char* AcquireAcceptBuffer() {
            std::lock_guard<std::mutex> lock(m_bufMutex);
            auto buf = m_acceptBuffers.front();
            m_acceptBuffers.pop();
            return buf;
        }
        void ReturnAcceptBuf(char*  buf) {
            std::lock_guard<std::mutex> lock(m_bufMutex);
            m_acceptBuffers.push(buf);
        }
    };
}
