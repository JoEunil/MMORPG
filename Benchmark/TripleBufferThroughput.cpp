#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdint>
#include <numeric>
#include <thread>
#include <vector>

#include <BaseLib/TripleBuffer.h>
#include "Percentile.h"
#include "TripleBufferSpinCounter.h"

namespace {
    struct Snapshot {
        uint64_t sequence = 0;
    };

    template <typename Buffer>
    void RunTripleBufferThroughput(benchmark::State& state) {
        const int64_t consumerCount = state.range(0);
        Buffer buffer;
        buffer.Init(new Snapshot(), new Snapshot());
        Snapshot* writer = new Snapshot();

        std::atomic<bool> started = false;
        std::atomic<bool> running = true;
        std::vector<uint64_t> readCounts(consumerCount);
        std::vector<std::thread> consumers;
        consumers.reserve(consumerCount);

        for (int64_t i = 0; i < consumerCount; ++i) {
            consumers.emplace_back([&, i] {
                while (!started.load(std::memory_order_acquire))
                    std::this_thread::yield();

                uint64_t reads = 0;
                while (running.load(std::memory_order_relaxed)) {
                    auto reader = buffer.Read();
                    benchmark::DoNotOptimize(reader.data);
                    ++reads;
                }
                readCounts[i] = reads;
            });
        }

        uint64_t writes = 0;
        started.store(true, std::memory_order_release);
        for (auto _ : state) {
            writer->sequence = ++writes;
            buffer.Write(writer);
        }

        running.store(false, std::memory_order_relaxed);
        for (auto& consumer : consumers)
            consumer.join();

        const uint64_t reads = std::accumulate(readCounts.begin(), readCounts.end(), uint64_t{0});
        delete writer;

        state.counters["writes/s"] = benchmark::Counter(static_cast<double>(writes), benchmark::Counter::kIsRate); // KIsRate  =  count / sec
        state.counters["reads/s"] = benchmark::Counter(static_cast<double>(reads), benchmark::Counter::kIsRate);
    }

    void BM_TripleBufferBusySpinThroughput(benchmark::State& state) {
        RunTripleBufferThroughput<BenchmarkSupport::TripleBufferSpinCounter<Snapshot>>(state);
    }

    void BM_TripleBufferYieldThroughput(benchmark::State& state) {
        RunTripleBufferThroughput<Base::TripleBuffer<Snapshot>>(state);
    }
}

BENCHMARK(BM_TripleBufferBusySpinThroughput)
    ->DenseRange(1, 4, 1)
    ->ArgName("consumers")
    ->UseRealTime()
    ->Unit(benchmark::kNanosecond)
    ->MinTime(3.0);

BENCHMARK(BM_TripleBufferYieldThroughput)
    ->DenseRange(1, 4, 1)
    ->ArgName("consumers")
    ->UseRealTime()
    ->Unit(benchmark::kNanosecond)
    ->MinTime(3.0);
