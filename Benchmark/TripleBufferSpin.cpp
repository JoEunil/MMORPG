#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "Percentile.h"
#include "TripleBufferSpinCounter.h"

namespace {
    struct Snapshot {
        uint64_t sequence = 0;
    };

    void BM_TripleBufferSpin(benchmark::State& state) {
        const int64_t consumerCount = state.range(0);
        BenchmarkSupport::TripleBufferSpinCounter<Snapshot> buffer;
        buffer.Init(new Snapshot(), new Snapshot());
        Snapshot* writer = new Snapshot();

        std::atomic<bool> started = false;
        std::atomic<bool> running = true;
        std::vector<std::thread> consumers;
        consumers.reserve(consumerCount);

        for (int64_t i = 0; i < consumerCount; ++i) {
            consumers.emplace_back([&] {
                while (!started.load(std::memory_order_acquire))
                    std::this_thread::yield();

                while (running.load(std::memory_order_relaxed)) {
                    auto reader = buffer.Read();
                    benchmark::DoNotOptimize(reader.data);
                }
            });
        }

        uint64_t writeCount = 0;
        std::vector<double> contendedSpins;

        started.store(true, std::memory_order_release);
        for (auto _ : state) {
            writer->sequence = ++writeCount;
            const uint32_t spinCount = buffer.Write(writer);
            if (spinCount != 0)
                contendedSpins.push_back(static_cast<double>(spinCount));
        }

        running.store(false, std::memory_order_relaxed);
        for (auto& consumer : consumers)
            consumer.join();

        delete writer;

        state.counters["contended_%"] = writeCount == 0 ? 0.0 : 100.0 * static_cast<double>(contendedSpins.size()) / static_cast<double>(writeCount);
        state.counters["spin_p95"] = P95(contendedSpins);
        state.counters["spin_p99"] = P99(contendedSpins);
        state.counters["spin_p100"] = P100(contendedSpins);
    }
}

//BENCHMARK(BM_TripleBufferSpin)
//    ->DenseRange(1, 4, 1)
//    ->ArgName("consumers")
//    ->UseRealTime()
//    ->Unit(benchmark::kNanosecond)
//    ->MinTime(3.0);
