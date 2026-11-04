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
    class OverlappedExPool {
        Base::FixedObjectPool<STOverlappedEx, OVERLAPPEDPOOL_SIZE> m_fixedPool;

        std::vector<char*> m_acceptBuffers; 
        std::mutex m_mutex;
        
        ~OverlappedExPool();
        bool IsReady() const {
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
        friend class Initializer;
    public:

        OverlappedExPool() {
            for (int i = 0; i < PREPOSTED_ACCEPTS * 3; i++)
                m_acceptBuffers.push_back(new char[ACCEPT_BUFFER_SIZE]);
        }

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
        size_t GetPoolSize() const {
            return m_fixedPool.GetPoolSize();
		}
    };
}
