#include "pch.h"
#include "ZoneThread.h"
#include "ZoneHandler.h"

#include <windows.h>
#undef min                  // Windows 매크로 제거
#undef max                  // Windows 매크로 제거

#include "CorePerfCollector.h"
#include "LoggerGlobal.h"
#include "ZoneState.h"

namespace Core {
    ZoneThread::~ZoneThread() {
        Stop();
        ProcessDeferredProfileUpdates(nullptr);
    }

    void ZoneThread::Initialize(uint16_t id, ZoneHandler& zoneHandler, CorePerfCollector& perf) {
        zoneID = id;
        handler = &zoneHandler;
        perfCollector = &perf;
    }

    bool ZoneThread::IsReady() const {
        if (handler == nullptr) {
            sysLogger->LogError("zone thread", "handler not initialized", "zoneID", zoneID);
            return false;
        }
        if (perfCollector == nullptr) {
            sysLogger->LogError("zone thread", "perfCollector not initialized", "zoneID", zoneID);
            return false;
        }
        if (!running.load(std::memory_order_relaxed)) {
            sysLogger->LogError("zone thread", "zone worker not running", "zoneID", zoneID);
            return false;
        }
        return true;
    }

    void ZoneThread::WorkerFunc(ZoneState& zone) {
        auto lastTick = std::chrono::steady_clock::now();
        auto lastDeltaSnapshot = lastTick;
        auto lastFullSnapshot = lastTick;

        std::stringstream ss;
        ss << std::this_thread::get_id();
        sysLogger->LogInfo("zone thread", "zone thread started", "threadID", ss.str());

        zone.InitializeMonster();
        while (running) {
            ProcessDeferredProfileUpdates(&zone);
            handler->RunTick(zone, zoneID, workQueue, 2000, *perfCollector, [&] {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastFullSnapshot >= FULL_SNAPSHOT_TICK) {
                    lastFullSnapshot = now;
                    lastDeltaSnapshot = now;
                    return ZoneHandler::TickSnapshot::Full;
                }
                if (now - lastDeltaSnapshot >= DELTA_SNAPSHOT_TICK) {
                    lastDeltaSnapshot = now;
                    return ZoneHandler::TickSnapshot::Delta;
                }
                return ZoneHandler::TickSnapshot::None;
            });

            const auto tickElapsed = std::chrono::steady_clock::now() - lastTick;
            const auto delay = tickElapsed - GAME_TICK;
            if (delay > std::chrono::milliseconds(50)) {
                sysLogger->LogWarn("zone thread", "tick delayed", "zoneID", zoneID,
                    "delay", std::chrono::duration_cast<std::chrono::milliseconds>(delay).count());
            }

            const auto sleepDur = GAME_TICK - tickElapsed;
            if (sleepDur > std::chrono::milliseconds(15))
                std::this_thread::sleep_for(sleepDur);

            lastTick += GAME_TICK; // 틱 지연 보정
        }
    }

    void ZoneThread::Start(ZoneState& zone) {
        if (running.exchange(true, std::memory_order_relaxed))
            return;

        try {
            thread = std::thread([this, &zone] { WorkerFunc(zone); });
        }
        catch (...) {
            running.store(false, std::memory_order_relaxed);
            throw;
        }

        const DWORD_PTR mask = 1ull << ((zoneID - 1) * 2);
        if (SetThreadAffinityMask(thread.native_handle(), mask) == 0)
            sysLogger->LogError("zone thread", "Failed to set thread affinity", "zoneID", zoneID);
    }

    void ZoneThread::Stop() {
        if (!running.exchange(false, std::memory_order_relaxed))
            return;
        if (thread.joinable())
            thread.join();
        if (sysLogger)
            sysLogger->LogInfo("zone thread", "zone thread stopped", "zoneID", zoneID);
    }

    bool ZoneThread::Enqueue(ZoneEvent& event) {
        if (perfCollector == nullptr)
            return false;
        if (event.type == ZoneEventType::ProfileVersion && !running.load(std::memory_order_acquire))
            return false;
        if (workQueue.push(event))
            return true;
        if (event.type == ZoneEventType::ProfileVersion) {
            // Cache has already committed the version. Keep it for the next tick if the queue is full.
            auto* head = deferredProfileUpdates.load(std::memory_order_relaxed);
            auto* entry = new (std::nothrow) DeferredProfileUpdate{event.profileVersion, head};
            if (entry == nullptr)
                return false;
            do {
                entry->next = head;
            } while (!deferredProfileUpdates.compare_exchange_weak(head, entry, std::memory_order_release,
                std::memory_order_relaxed));
            return true;
        }
        perfCollector->AddZoneDropCnt(zoneID);
        return false;
    }

    void ZoneThread::ProcessDeferredProfileUpdates(ZoneState* zone) {
        // Versions are monotonic, so processing this batch before older queued updates is safe.
        auto* entry = deferredProfileUpdates.exchange(nullptr, std::memory_order_acquire);
        while (entry != nullptr) {
            const std::unique_ptr<DeferredProfileUpdate> update(entry);
            entry = entry->next;
            if (zone != nullptr)
                handler->ApplyProfileVersion(*zone, update->update);
        }
    }
}
