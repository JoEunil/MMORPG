#pragma once

#include <thread>
#include <memory>
#include <mutex>
#include <array>
#include <condition_variable>

#include "ZoneHandler.h"
#include "IPacketView.h"
#include "Config.h"

#include <BaseLib/LockFreeQueueUP.h>

namespace Core {
    class IPacketView;
    class CorePerfCollector;
    struct Thread {
        std::thread thread;
        Base::LockFreeQueueUP<std::unique_ptr<IPacketView, PacketViewDeleter>, ZONE_QUEUE_SIZE> workQueue; // 개별 작업 큐
        std::atomic<bool> running = false;
    };

    class ZoneThreadSet{
        std::array<Thread, ZONE_COUNT> m_threads; // Thread 구조체 내부의 mutex, cv 때문에 이동생성자를쓸 수 없음 (vector 같은 컨테이너 사용불가)

        inline static ZoneHandler* handler;
        inline static CorePerfCollector* perfCollector;
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
