#include <CoreLib/pch.h>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <thread>
#include <benchmark/benchmark.h>
#include <CoreLib/Config.h>
#include <CoreLib/IPacketView.h>
#include <CoreLib/ZoneHandler.h>
#include <CoreLib/BroadcastThreadPool.h>
#include <NetLibrary/Config.h>
#include <NetLibrary/PacketPool.h>
#include <windows.h>

namespace Core {
    class ZoneBenchmarkAccess {
    public:
        void Init(StateManager& states, ZoneHandler& handler, ZoneState& zone, PacketWriter& writer, IPacketPool& pool, IPacketPool& bigPool, IPacketPool& giantPool, BroadcastThreadPool& broadcast, CorePerfCollector& perf, size_t players, bool clustered) {
            states.m_running.store(false);
            handler.Initialize(&states);
            writer.Initialize(&pool, &bigPool, &giantPool);
            broadcast.perfCollector = &perf;
            broadcast.m_running.store(true);
            ZoneState::Initialize(&broadcast, &writer, &states, &perf);
            for (size_t i = 0; i < players; ++i) {
                CharacterState c{};
                c.sessionID = i + 1;
                c.zoneInternalID = static_cast<uint32_t>(i + 1);
                c.hp = c.maxHp = 100000000;
                c.mp = c.maxMp = 100000000;
                c.attack = 10;
                c.skillSlotCnt = 3;
                c.skillSlot = {{0, 0}, {1, 0}, {2, 0}};
                c.x = clustered ? 50.0f : 2.0f + static_cast<float>(i % 20) * 4.5f;
                c.y = clustered ? 50.0f : 2.0f + static_cast<float>((i / 20) % 20) * 4.5f;
                auto [x, y] = GetCell(c.x, c.y, zone.m_area);
                zone.AddToCell(c, x, y);
                zone.m_sessionToIndex[c.sessionID] = static_cast<uint16_t>(i);
                zone.m_InternalIDToIndex[c.zoneInternalID] = static_cast<uint16_t>(i);
                zone.m_chars.push_back(c);
            }
            zone.InitializeMonster();
        }
        static void Bind(StateManager& states, std::unique_ptr<ZoneState> zone) {
            states.m_states.emplace(1, std::move(zone));
        }

        static void Cleanup(ZoneState& zone) {
            delete zone.sessionSnapshotWriter;
            zone.sessionSnapshotWriter = nullptr;
            ZoneState::Initialize(nullptr, nullptr, nullptr, nullptr);
        }

        static size_t DrainSnapshots(BroadcastThreadPool& broadcast, ZoneState& zone, bool full) {
            const int batches = full || !(zone.m_deltaTickCounter & 1) ? 3 : 2;
            size_t characterFields = 0;
            for (int i = 0; i < batches; ++i) {
                BroadcastThreadPool::BroadcastWork work;
                if (!broadcast.m_workQ.pop(work)) throw std::runtime_error("snapshot queue closed");
                if (i == 0) for (auto& chunk : work->second)
                    if (chunk) characterFields += chunk->GetCount();
            }
            return characterFields;
        }
    };
}

namespace {
    constexpr size_t TICKS = 1200;

    class ThreadAffinity {
        DWORD_PTR previous = 0;
    public:
        ThreadAffinity() {
            DWORD_PTR processMask = 0, systemMask = 0;
            if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) && processMask) {
                // Zone 1 uses CPU 0 in production; use the first allowed CPU here.
                const DWORD_PTR mask = processMask & (~processMask + 1);
                previous = SetThreadAffinityMask(GetCurrentThread(), mask);
            }
        }
        ~ThreadAffinity() {
            if (previous) SetThreadAffinityMask(GetCurrentThread(), previous);
        }
        bool IsPinned() const { return previous != 0; }
        ThreadAffinity(const ThreadAffinity&) = delete;
        ThreadAffinity& operator=(const ThreadAffinity&) = delete;
    };

    using Pool = Net::PacketPool<128>;
    class Action final : public Core::IPacketView {
    public:
        Core::PacketStruct<Core::ActionRequestBody> data{};
        uint64_t session = 0;
        uint64_t GetSessionID() const override { return session; }
        uint8_t* GetPtr() const override { return reinterpret_cast<uint8_t*>(const_cast<decltype(data)*>(&data)); }
        uint16_t GetLength() const override { return sizeof(data); }
        uint8_t GetOpcode() const override { return Core::OP::ACTION; }
        void Release() override {} // fixture owns the preallocated views
    };

    void BM_ZoneTick(benchmark::State& state) {
        const size_t players = static_cast<size_t>(state.range(0));
        const bool clustered = state.range(1) != 0;
        const size_t moves = static_cast<size_t>(state.range(2));
        ThreadAffinity affinity;
        if (!affinity.IsPinned()) {
            state.SkipWithError("could not pin benchmark thread");
            return;
        }
        Core::ZoneBenchmarkAccess fixture;
        Core::StateManager states;
        Core::ZoneHandler handler;
        Core::PacketWriter writer;
        Core::BroadcastThreadPool broadcast;
        Core::CorePerfCollector perf{};
        Pool pool(Net::NORMAL_PACKET_LENGTH), bigPool(Net::BIG_PACKET_LENGTH), giantPool(Net::GIANT_PACKET_LENGTH);
        auto zone = std::make_unique<Core::ZoneState>(1);
        auto* z = zone.get();
        std::srand(42);
        fixture.Init(states, handler, *z, writer, pool, bigPool, giantPool, broadcast, perf, players, clustered);
        Core::ZoneBenchmarkAccess::Bind(states, std::move(zone));
        Base::LockFreeQueue<std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter>, Core::ZONE_QUEUE_SIZE> queue;
        std::vector<Action> actions(players * (moves + 3));
        for (size_t i = 0; i < actions.size(); ++i) {
            auto& a = actions[i];
            a.session = i % players + 1;
            a.data.header.magic = Core::MAGIC;
            a.data.header.opcode = Core::OP::ACTION;
            a.data.header.length = sizeof(a.data);
            a.data.body.skillSlot = i < players * moves ? 255 : static_cast<uint8_t>(i / players - moves);
        }
        std::vector<double> times;
        times.reserve(TICKS);
        size_t tick = 0, overBudget = 0;
        const auto runStart = std::chrono::steady_clock::now();
        for (auto _ : state) {
            const bool burst = tick % 20 == 0;
            const bool full = (tick + 1) % 20 == 0;
            const size_t packets = players * (moves + (burst ? 3 : 0));
            // Split the same 0.25-unit movement into distinct targets; skill packets repeat the final target.
            for (size_t i = 0; i < packets; ++i) {
                auto& a = actions[i];
                const size_t player = i % players;
                a.data.body.dir = tick % 2 == 0 ? 1 : 3;
                const size_t batch = i / players;
                const float fraction = static_cast<float>((std::min)(batch + 1, moves)) / moves;
                const float offset = tick % 2 == 0 ? 0.25f * fraction : 0.25f * (1.0f - fraction);
                a.data.body.x = (clustered ? 50.0f : 2.0f + static_cast<float>(player % 20) * 4.5f)
                    + offset;
                a.data.body.y = clustered ? 50.0f : 2.0f + static_cast<float>((player / 20) % 20) * 4.5f;
                std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter> p(&a);
                if (!queue.push(std::move(p))) throw std::runtime_error("zone prefill failed");
            }
            const auto start = std::chrono::steady_clock::now();
            // Same tick implementation as the server; process all offered packets.
            const size_t processed = handler.RunTick(1, queue, packets, perf, [full] {
                return full ? Core::ZoneHandler::TickSnapshot::Full : Core::ZoneHandler::TickSnapshot::Delta;
            });
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            state.SetIterationTime(seconds);
            const double ms = seconds * 1000.0;
            times.push_back(ms);
            if (ms > 50.0) ++overBudget;
            const auto fields = Core::ZoneBenchmarkAccess::DrainSnapshots(broadcast, *z, full);
            if (full && fields != players) {
                state.SkipWithError("full snapshot omitted characters"); break;
            }
            if (processed != packets) { state.SkipWithError("unexpected processed count"); break; }
            ++tick;
            std::this_thread::sleep_until(runStart + Core::GAME_TICK * tick);
        }
        std::sort(times.begin(), times.end());
        if (!times.empty()) {
            const auto percentile = [&](double p) {
                return times[static_cast<size_t>(std::ceil(p * times.size())) - 1];
            };
            state.counters["max_tick_ms"] = times.back();
            state.counters["p99_tick_ms"] = percentile(0.99);
            state.counters["over_50ms_pct"] = 100.0 * overBudget / times.size();
        }
        Core::ZoneBenchmarkAccess::Cleanup(*z);
    }
    void ZoneArgs(benchmark::Benchmark* b) {
        for (int clustered : {0, 1})
            for (int moves : {1, 2, 3, 4})
                b->Args({2000, clustered, moves});
    }
}
//BENCHMARK(BM_ZoneTick)->Apply(ZoneArgs)
//    ->ArgNames({"players", "clustered", "moves"})
//    ->UseManualTime()
//    ->Iterations(TICKS)
//    ->Repetitions(1)
//    ->Unit(benchmark::kMillisecond);
