#pragma once

#include <thread>
#include <memory>
#include <mutex>
#include <array>
#include <new>
#include <condition_variable>
#include <atomic>
#include <cstdint>

#include "LoggerGlobal.h"
#include "IPacketView.h"
#include "WorldConfig.h"

#include <BaseLib/MPMCQueue.h>

namespace Core {
    class IPacketView;
    class CorePerfCollector;
    class ZoneHandler;

    enum class ZoneEventType : uint8_t {
        Packet,
    };

    struct ZoneEvent {
        ZoneEventType type = ZoneEventType::Packet;
        std::unique_ptr<IPacketView, PacketViewDeleter> packet;
    };

    struct alignas(std::hardware_destructive_interference_size) Thread {
        std::thread thread;
        Base::MPMCQueue<ZoneEvent, ZONE_QUEUE_SIZE> workQueue;
        std::atomic<bool> running = false;
    };

    class ZoneThreadSet {
        std::array<Thread, ZONE_COUNT> m_threads;

        inline static ZoneHandler* handler;
        inline static CorePerfCollector* perfCollector;
        inline static std::atomic<bool> m_running;
        static void Initialize(ZoneHandler* c, CorePerfCollector* p) {
            handler = c;
            perfCollector = p;
        }
        static bool IsReady() {
            if (handler == nullptr) {
                sysLogger->LogError("zone thread", "handler not initialized");
                return false;
            }
            if (perfCollector == nullptr) {
                sysLogger->LogError("zone thread", "perfCollector not initialized");
                return false;
            }
            return true;
        }
        void Start();
        void Stop();

        void WorkerFunc(Thread* t, int zoneId);
        friend class Initializer;
    public:
        ~ZoneThreadSet() {
            Stop();
        }
        void EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter> pv, uint16_t zoneID);// 공용 큐에 작업 추가
    };
}
