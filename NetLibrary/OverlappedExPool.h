#pragma once
#include <deque>
#include <mutex>
#include <cstdint>
#include <vector>
#include "STOverlappedEx.h"
#include "Config.h"


namespace Net {
    static constexpr size_t ACCEPT_BUFFER_SIZE = (sizeof(SOCKADDR_IN) + 16) * 2;
    class PacketPool;
    class OverlappedExPool {
        std::deque<STOverlappedEx*> m_overlappedPool;

        std::vector<char*> m_acceptBuffers; // LIFO로 관리하면 충분
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
        PacketPool* packetPool;
        friend class Initializer;
    public:
        STOverlappedEx* Acquire();
        void Return(STOverlappedEx*);
        char* AcquireAcceptBuffer() {
            std::lock_guard<std::mutex> lock(m_bufMutex);
            if (m_acceptBuffers.empty()) {
                return nullptr;
            }
            auto buf = m_acceptBuffers.back();
            m_acceptBuffers.pop_back();
            return buf;
        }
        void ReturnAcceptBuf(char*  buf) {
            std::lock_guard<std::mutex> lock(m_bufMutex);
            m_acceptBuffers.push_back(buf);
        }
    };
}
