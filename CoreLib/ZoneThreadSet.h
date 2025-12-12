#pragma once

#include <thread>
#include <memory>
#include <queue>
#include <mutex>
#include <array>
#include <condition_variable>

#include "ZoneHandler.h"
#include "IPacketView.h"
#include "Config.h"

namespace Core {
    class ILogger;
    class IPacketView;
    struct Thread {
        std::thread thread;
        std::queue<std::shared_ptr<IPacketView>> workQueue; // 개별 작업 큐
        // lock-free queue쓰면 성능 향상
        std::mutex mutex;
        std::atomic<bool> running = false;
    };

    class ZoneThreadSet{
        std::array<Thread, ZONE_COUNT> m_threads; // Thread 구조체 내부의 mutex, cv 때문에 이동생성자를쓸 수 없음 (vector 같은 컨테이너 사용불가)
        std::mutex m_mutex;

        inline static ZoneHandler* handler;
        inline static ILogger* logger;
        static void Initialize(ZoneHandler* c, ILogger* l) {
            handler = c;
            logger = l;
        }
        void Start();
        void Stop();
        bool IsReady() {
            return m_threads.size() == ZONE_COUNT && handler != nullptr;
        }

        void WorkerFunc(Thread* t, int zoneId);
        friend class Initializer;
    public:
        ~ZoneThreadSet() {
            Stop();
        }
        void EnqueueWork(std::shared_ptr<IPacketView> pv, uint16_t zoneID);// 공용 큐에 작업 추가, 해시(zoneID)로 작업 분배
    };
}
