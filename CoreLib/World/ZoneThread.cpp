#include "pch.h"
#include "ZoneThread.h"
#include "ZoneHandler.h"
#include "ZoneTickRunner.h"

#include <windows.h>
#undef min                  // Windows 매크로 제거
#undef max                  // Windows 매크로 제거

#include "../CorePerfCollector.h"
#include "../LoggerGlobal.h"
#include "ZoneState.h"

namespace Core {
    ZoneThread::~ZoneThread() {
        Stop();
    }

    void ZoneThread::Initialize(ZoneState& state, ZoneTickRunner& runner, ZoneHandler& zoneHandler, CorePerfCollector& perf) {
        zoneID = state.GetZoneID();
        zone = &state;
        tickRunner = &runner;
        handler = &zoneHandler;
        perfCollector = &perf;
    }

    bool ZoneThread::IsReady() const {
        if (zone == nullptr) {
            sysLogger->LogError("zone thread", "zone state not initialized", {{"zoneID", zoneID}});
            return false;
        }
        if (tickRunner == nullptr) {
            sysLogger->LogError("zone thread", "tick runner not initialized", {{"zoneID", zoneID}});
            return false;
        }
        if (handler == nullptr) {
            sysLogger->LogError("zone thread", "handler not initialized", {{"zoneID", zoneID}});
            return false;
        }
        if (perfCollector == nullptr) {
            sysLogger->LogError("zone thread", "perfCollector not initialized", {{"zoneID", zoneID}});
            return false;
        }
        if (!running.load(std::memory_order_relaxed)) {
            sysLogger->LogError("zone thread", "zone worker not running", {{"zoneID", zoneID}});
            return false;
        }
        return true;
    }

    void ZoneThread::WorkerFunc() {
        auto lastTick = std::chrono::steady_clock::now();
        auto lastDeltaSnapshot = lastTick;
        auto lastFullSnapshot = lastTick;

        std::stringstream ss;
        ss << std::this_thread::get_id();
        sysLogger->LogInfo("zone thread", "zone thread started", {{"threadID", ss.str()}});

        zone->InitializeMonster();
        while (running) {
            tickRunner->RunTick(workQueue, 2000, [&] {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastFullSnapshot >= FULL_SNAPSHOT_TICK) {
                    lastFullSnapshot = now;
                    lastDeltaSnapshot = now;
                    return ZoneTickRunner::TickSnapshot::Full;
                }
                if (now - lastDeltaSnapshot >= DELTA_SNAPSHOT_TICK) {
                    lastDeltaSnapshot = now;
                    return ZoneTickRunner::TickSnapshot::Delta;
                }
                return ZoneTickRunner::TickSnapshot::None;
            });

            const auto tickElapsed = std::chrono::steady_clock::now() - lastTick;
            const auto delay = tickElapsed - GAME_TICK;
            if (delay > std::chrono::milliseconds(50)) {
                sysLogger->LogWarn("zone thread", "tick delayed", {{"zoneID", zoneID},
                    {"delay", std::chrono::duration_cast<std::chrono::milliseconds>(delay).count()}});
            }

            const auto sleepDur = GAME_TICK - tickElapsed;
            if (sleepDur > std::chrono::milliseconds(15))
                std::this_thread::sleep_for(sleepDur);

            lastTick += GAME_TICK; // 틱 지연 보정
        }
        ZoneEvent event;
        while (workQueue.Dequeue(event)) {
            if (const auto* disconnect = std::get_if<ZoneDisconnectEvent>(&event))
                handler->ProcessDisconnect(*zone, *disconnect);
        }
        handler->SaveRemainingCharacters(*zone);
    }

    void ZoneThread::Start() {
        if (running.exchange(true, std::memory_order_relaxed))
            return;

        try {
            thread = std::thread(&ZoneThread::WorkerFunc, this);
        }
        catch (...) {
            running.store(false, std::memory_order_relaxed);
            throw;
        }

        const DWORD_PTR mask = 1ull << ((zoneID - 1) * 2);
        if (SetThreadAffinityMask(thread.native_handle(), mask) == 0)
            sysLogger->LogError("zone thread", "Failed to set thread affinity", {{"zoneID", zoneID}});
    }

    void ZoneThread::Stop() {
        if (!running.exchange(false, std::memory_order_relaxed))
            return;
        if (thread.joinable())
            thread.join();
        if (sysLogger)
            sysLogger->LogInfo("zone thread", "zone thread stopped", {{"zoneID", zoneID}});
    }

    bool ZoneThread::CanEnqueue(const ZoneEvent& event) const {
        return perfCollector != nullptr &&
            (std::holds_alternative<ZonePacketEvent>(event) || running.load(std::memory_order_acquire));
    }

    bool ZoneThread::Enqueue(ZoneEvent& event) {
        if (!CanEnqueue(event))
            return false;
        const bool accepted = workQueue.Enqueue(event) != Base::EnqueueResult::Rejected;
        if (!accepted)
            perfCollector->AddZoneDropCnt(zoneID);
        return accepted;
    }

    bool ZoneThread::EnqueueDeferred(ZoneEvent& event) {
        if (!CanEnqueue(event))
            return false;
        const bool accepted = workQueue.EnqueueDeferred(event) != Base::EnqueueResult::Rejected;
        if (!accepted)
            perfCollector->AddZoneDropCnt(zoneID);
        return accepted;
    }
}
