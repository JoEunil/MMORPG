#pragma once
#include <deque>
#include <mutex>
#include <cstdint>
#include <vector>
#include "STOverlappedEx.h"
#include "Config.h"
#include <CoreLib/LoggerGlobal.h>
#include <BaseLib/FixedObjectPool.h>

namespace Net {
    static constexpr size_t ACCEPT_BUFFER_SIZE = (sizeof(SOCKADDR_IN) + 16) * 2;
    class PacketPool;
    class OverlappedExPool {
        Base::FixedObjectPool<STOverlappedEx, OVERLAPPEDPOOL_SIZE> m_fixedPool;

        std::vector<char*> m_acceptBuffers; // LIFO로 관리하면 충분
        std::mutex m_mutex;
        
        ~OverlappedExPool();
        void Initialize();
        bool IsReady() {
            if (m_acceptBuffers.empty()) {
                Core::sysLogger->LogError("ovelapped pool", "m_acceptBuffers not initialized");
                return false;
            }
            if (m_fixedPool.GetPoolSize() < OVERLAPPEDPOOL_SIZE / 2) {
                Core::sysLogger->LogError("ovelapped pool", "m_fixedPool not initialized");
                return false;
			}
            return true;
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
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_acceptBuffers.empty()) {
                return nullptr;
            }
            auto buf = m_acceptBuffers.back();
            m_acceptBuffers.pop_back();
            return buf;
        }
        void ReturnAcceptBuf(char*  buf) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_acceptBuffers.push_back(buf);
        }
        size_t GetPoolSize() {
            return m_fixedPool.GetPoolSize();
		}
    };
}
