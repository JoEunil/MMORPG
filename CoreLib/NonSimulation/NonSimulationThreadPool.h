#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "NonSimulationDispatcher.h"
#include "NonSimulationEvent.h"
#include "../World/Interface/ILobbyReturnQueue.h"
#include "Interface/INonSimulationQueue.h"
#include "../Interface/IPacketView.h"
#include "NonSimulationConfig.h"
#include <BaseLib/BlockingBackPressureQueue.h>
#include "../LoggerGlobal.h"
namespace Core {
    // 게임틱 단위로 처리되지 않는 (zone 상태와 관련 없는) 요청 처리
    class NonSimulationThreadPool final : public INonSimulationQueue, public ILobbyReturnQueue {
        std::vector<std::thread> m_threads;
        Base::BlockingBackPressureQueue<NonSimulationEvent, NON_SIMULATION_EVENT_QUEUE_SIZE, NON_SIMULATION_DEFER_QUEUE_SIZE,true> m_eventQueue;

        std::atomic<bool> m_running = false;
        
        NonSimulationDispatcher* dispatcher = nullptr;
        void Initialize(NonSimulationDispatcher* h) { dispatcher = h; }
        void Start();
        void Stop();
        bool IsReady() {
            if (m_threads.size() != NON_SIMULATION_THREADPOOL_SIZE) {
                sysLogger->LogError("non zone thread", "m_threads not initialized");
                return false;
            }
            if (dispatcher == nullptr) {
                sysLogger->LogError("non zone thread", "dispatcher not initialized");
                return false;
            }
            return true;
        }
        void WorkFunc();
        friend class Initializer;
    public:
        ~NonSimulationThreadPool() override {
            Stop();
        }
        void EnqueuePacket(std::unique_ptr<IPacketView, PacketViewDeleter> packet) override;
        void EnqueueDisconnect(uint64_t sessionID) override;
        void EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) override;
    };
}
