#include "pch.h"
#include "ZoneThreadSet.h"

#include <windows.h>
#include <algorithm>
#undef min                  // Windows 매크로 제거
#undef max                  // Windows 매크로 제거

#include "ZoneHandler.h"
#include "IPacketView.h"
#include "PacketTypes.h"
#include "ILogger.h"
#include "Config.h"

namespace Core {
    void ZoneThreadSet::WorkerFunc(Thread* t, int zoneID) {
        std::queue<std::unique_ptr<IPacketView, PacketViewDeleter>> localQueue;
        auto lastTick = std::chrono::steady_clock::now();
        auto lastDeltaSnapshot = lastTick;
        auto lastFullSnapshot = lastTick;

        logger->LogInfo(std::format("zone thread started id {}", zoneID));
        while (t->running) {
            {
                std::lock_guard<std::mutex> lock(t->mutex);
                std::swap(localQueue, t->workQueue);
                // workQueue 루프에서 mutex 잡으면 Enqueu에서 wait 되고, process할 때 unlock 해버리면 틱이 늘어질 수 있음.
            }

            while(!localQueue.empty())
            {
                auto& work = localQueue.front();
                handler->Process(work.get(), zoneID); // 게임 상태 업데이트 처리
                localQueue.pop();
            }
            handler->FlushCheat(zoneID);
            auto now = std::chrono::steady_clock::now();
            auto deltaSnapshotElapsed = now - lastDeltaSnapshot;
            auto fullSnapshotElapsed = now - lastFullSnapshot;
            if (fullSnapshotElapsed >= FULL_SNAPSHOT_TICK) {
                lastFullSnapshot = now;
                handler->BroadcastFullState(zoneID);
            }
            else if (deltaSnapshotElapsed >= DELTA_SNAPSHOT_TICK) {
                lastDeltaSnapshot = now;
                handler->BroadcastDeltaState(zoneID);
            }

            now = std::chrono::steady_clock::now();
            auto tickElapsed = now - lastTick;

            #ifdef _DEBUG
            auto delay = tickElapsed - GAME_TICK;
            if (delay > std::chrono::milliseconds(10)) {
                logger->LogWarn(std::format("zone: {}, tick delayed: {}", zoneID, std::chrono::duration_cast<std::chrono::milliseconds>(delay)));
            }
            #endif

            // --- 틱 간격 유지 ---
            if (tickElapsed < GAME_TICK) {
                std::this_thread::sleep_for(GAME_TICK - tickElapsed);
            }
                
            lastTick += GAME_TICK; // 틱 지연 보정
            //                lastTick = std::chrono::steady_clock::now(); // 밀린 틱 무시
        }
        
        
        while(!localQueue.empty())
        {
            auto& work = localQueue.front();
            handler->Process(work.get(), zoneID); // 게임 상태 업데이트 처리
            localQueue.pop();
        }
        
        std::lock_guard<std::mutex> lock(t->mutex);
        while(!t->workQueue.empty())
        {
            auto& work = t->workQueue.front();
            handler->Process(work.get(), zoneID); // 게임 상태 업데이트 처리
            t->workQueue.pop();
        }
    }

    void ZoneThreadSet::Start() {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (int i = 0; i < ZONE_COUNT; i++)
        {
            //0번 lobby zone은 noneZoneThreadPool에서만 처리
            m_threads[i].running.store(true);
            m_threads[i].thread = std::thread([this, i, &thread = m_threads[i]]() {
                this->WorkerFunc(&thread, i+1);
                });
            DWORD_PTR mask = 1ull << (i);
            DWORD_PTR prevMask = SetThreadAffinityMask((HANDLE)m_threads[i].thread.native_handle(), mask); // 스레드 코어 고정
            
            if (prevMask == 0) {
                logger->LogError("Failed to set thread affinity for zone " + std::to_string(i+1));
            }
            
            //thread 우선순위 설정, std::thread에서는 할 수없어서 win32 API 통해서
            HANDLE h = (HANDLE)m_threads[i].thread.native_handle();
            // HIGH PRIORITY
            if (!::SetThreadPriority(h, THREAD_PRIORITY_HIGHEST)) {
                logger->LogError("Failed to set priority, zone {} thread" + std::to_string(i+1));
            }
            
        }
    }

    void ZoneThreadSet::Stop() {
        for (auto& t : m_threads)
        {
            {
                std::lock_guard<std::mutex> lock(t.mutex);
                t.running = false;
            }
            if (t.thread.joinable())
                t.thread.join();
        }
    }

    void ZoneThreadSet::EnqueueWork(std::unique_ptr<Core::IPacketView, PacketViewDeleter> pv, uint16_t zoneID) {
        Thread& t = m_threads[zoneID-1];
        std::lock_guard<std::mutex> lock(t.mutex);
        t.workQueue.emplace(std::move(pv));
    }
}

