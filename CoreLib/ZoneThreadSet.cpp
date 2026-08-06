#include "pch.h"
#include "ZoneThreadSet.h"

#include <windows.h>
#include <algorithm>
#undef min                  // Windows 매크로 제거
#undef max                  // Windows 매크로 제거

#include "ZoneHandler.h"
#include "IPacketView.h"
#include "PacketTypes.h"
#include "LoggerGlobal.h"
#include "CorePerfCollector.h"
#include "Config.h"

namespace Core {
    void ZoneThreadSet::WorkerFunc(Thread* t, int zoneID) {
        std::queue<std::unique_ptr<IPacketView, PacketViewDeleter>> localQueue;
        auto lastTick = std::chrono::steady_clock::now();
        auto lastDeltaSnapshot = lastTick;
        auto lastFullSnapshot = lastTick;

        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("zone thread", "zone thread started", "threadID", ss.str());

        handler->SpawnMonster(zoneID);
        while (t->running) {
            handler->SkillCoolDown(zoneID);
            handler->ReplenishMoveBudget(zoneID);
            std::unique_ptr<IPacketView, PacketViewDeleter> packet;
            const size_t MAX_PACKETS_PER_TICK = 2000;
            size_t processed = 0;
            bool got = t->workQueue.pop(packet);
            while(got && processed < MAX_PACKETS_PER_TICK)
            {
                handler->Process(packet.get(), zoneID); // 게임 상태 업데이트 처리
                got = t->workQueue.pop(packet);
                processed++;
            }
            perfCollector->AddPacketProcessCnt(zoneID, processed);
            handler->ApplySkill(zoneID);
            handler->UpdateMonster(zoneID);
            handler->FlushCheat(zoneID);
            auto now = std::chrono::steady_clock::now();
            auto deltaSnapshotElapsed = now - lastDeltaSnapshot;
            auto fullSnapshotElapsed = now - lastFullSnapshot;
            if (fullSnapshotElapsed >= FULL_SNAPSHOT_TICK) {
                lastFullSnapshot = now;
                lastDeltaSnapshot = now;
                handler->BroadcastFullState(zoneID);
            }
            else if (deltaSnapshotElapsed >= DELTA_SNAPSHOT_TICK) {
                lastDeltaSnapshot = now;
                handler->BroadcastDeltaState(zoneID);
            }

            now = std::chrono::steady_clock::now();
            auto tickElapsed = now - lastTick;

            auto delay = tickElapsed - GAME_TICK;
            if (delay > std::chrono::milliseconds(50)) {
                sysLogger->LogWarn("zone thread", "tick delayed", "zoneID",  zoneID, "delay", std::chrono::duration_cast<std::chrono::milliseconds>(delay).count());
            }

            auto sleepDur = GAME_TICK - tickElapsed;
            if (sleepDur > std::chrono::milliseconds(15)) {
                std::this_thread::sleep_for(sleepDur);
            }
                
            lastTick += GAME_TICK; // 틱 지연 보정
            //                lastTick = std::chrono::steady_clock::now(); // 밀린 틱 무시
        }
        
    }

    void ZoneThreadSet::Start() {
        m_running.store(true, std::memory_order_relaxed);
        for (int i = 0; i < ZONE_COUNT; i++)
        {
            //0번 lobby zone은 nonZoneThreadPool에서만 처리
            m_threads[i].running.store(true, std::memory_order_relaxed);
            m_threads[i].thread = std::thread([this, i, &thread = m_threads[i]]() {
                this->WorkerFunc(&thread, i+1);
                });
            DWORD_PTR mask = 1ull << (i*2);
            DWORD_PTR prevMask = SetThreadAffinityMask((HANDLE)m_threads[i].thread.native_handle(), mask); // 스레드 코어 고정
            
            if (prevMask == 0) {
                sysLogger->LogError("zone thread", "Failed to set thread affinity", "zoneID", i+1);
            }
            
            //thread 우선순위 설정, std::thread에서는 할 수없어서 win32 API 통해서
            HANDLE h = (HANDLE)m_threads[i].thread.native_handle();
            // HIGH PRIORITY
            //if (!::SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL)) {
            //    sysLogger->LogError("zone thread", "Failed to set priority", "zoneID", i + 1);
            //}
            
        }
    }

    void ZoneThreadSet::Stop() {
        // 이미 Stop 되었으면 재호출은 무시 (graceful shutdown + 소멸자 이중 호출 방어)
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
        for (auto& t : m_threads)
        {
            t.running.store(false, std::memory_order_relaxed);
            if (t.thread.joinable())
                t.thread.join();
        }
        // 소멸자 경로에서는 로거가 이미 파괴되었을 수 있음
        if (sysLogger)
            sysLogger->LogInfo("zone thread", "zone thread stopped");
    }

    void ZoneThreadSet::EnqueueWork(std::unique_ptr<Core::IPacketView, PacketViewDeleter> pv, uint16_t zoneID) {
        Thread& t = m_threads[zoneID-1];
        if (!t.workQueue.push(std::move(pv))) {
            // 실패(full) 시 false 반환
            perfCollector->AddZoneDropCnt(zoneID);
        }
    }
}

