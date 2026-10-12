#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "NonZoneHandler.h"
#include "IPacketView.h"
#include "Config.h"
#include <BaseLib/BlockingBackPressureQueue.h>
#include "LoggerGlobal.h"
namespace Core {
    // 게임틱 단위로 처리되지 않는 (zone 상태와 관련 없는) 요청 처리
    class NonZoneThreadPool {
        enum class EventType : uint8_t {
            Work,
            Disconnect,
        };

        struct Event {
            EventType type = EventType::Work;
            std::unique_ptr<IPacketView, PacketViewDeleter> work;
            uint64_t sessionID = 0;
        };

        std::vector<std::thread> m_threads;
        Base::BlockingBackPressureQueue<Event, NON_ZONE_EVENT_QUEUE_SIZE, NON_ZONE_DEFER_QUEUE_SIZE,true> m_eventQueue;

        std::atomic<bool> m_running = false;
        
        NonZoneHandler* handler = nullptr;
        void Initialize(NonZoneHandler* h) {
            handler = h;
        }
        void Start();
        void Stop();
        bool IsReady() {
            if (m_threads.size() != NON_ZONE_THREADPOOL_SIZE) {
                sysLogger->LogError("none zone thread", "m_threads not initialized");
                return false;
            }
            if (handler == nullptr) {
                sysLogger->LogError("none zone thread", "handler not initialized");
                return false;
            }
            return true;
        }
        void WorkFunc();
        friend class Initializer;
    public:
        ~NonZoneThreadPool() {
            Stop();
        }
        void EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv);
        void EnqueueDisconnect(uint64_t sessionID);
    };
}
