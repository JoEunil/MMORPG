#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <BaseLib/WAL.h>
#include "Percentile.h"

namespace {
#pragma pack(push, 1)
    struct WalRecord {
        uint64_t sequence;
        uint32_t value32;
        uint16_t value16;
        uint8_t value8;
    };
#pragma pack(pop)

    constexpr uint16_t WAL_RECORD_TYPE = 1;

    void ApplyEmpty(const Base::WALHeader&, const uint8_t*) {
    }

    void CleanupWalFiles(const std::filesystem::path& basePath) {
        const auto directory = basePath.parent_path();
        const auto prefix = basePath.filename().string() + ".";

        std::error_code ec;
        std::filesystem::directory_iterator it(directory, ec);
        const std::filesystem::directory_iterator end;
        while (!ec && it != end) {
            const auto name = it->path().filename().string();
            if (name.rfind(prefix, 0) == 0)
                std::filesystem::remove(it->path(), ec);
            it.increment(ec);
        }
    }

    struct WalFile {
        std::filesystem::path basePath;

        ~WalFile() {
            CleanupWalFiles(basePath);
        }
    };

    std::filesystem::path MakeWalBasePath(int64_t fsyncIntervalMs, int64_t writerCount) {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        auto directory = std::filesystem::temp_directory_path() / "GameBenchmarks";
        std::filesystem::create_directories(directory);
        return directory / ( "wal_fsync_" + std::to_string(fsyncIntervalMs) + "ms_"  + std::to_string(writerCount) + "writers_" + std::to_string(nonce));
    }

    void BM_WalFsyncInterval(benchmark::State& state) {
        const int64_t fsyncIntervalMs = state.range(0);
        const int64_t writerCount = state.range(1);
        if (fsyncIntervalMs <= 0 || writerCount <= 0) {
            state.SkipWithError("fsync interval and writer count must be positive");
            return;
        }

        WalFile files(MakeWalBasePath(fsyncIntervalMs, writerCount));
        Base::WAL wal(files.basePath.string(), UINT32_MAX, ApplyEmpty);

        std::atomic<bool> started = false;
        std::atomic<bool> running = true;
        std::vector<uint64_t> successfulWrites(static_cast<size_t>(writerCount));
        std::vector<uint64_t> failedWrites(static_cast<size_t>(writerCount));
        std::vector<std::thread> writers;
        writers.reserve(static_cast<size_t>(writerCount));

        for (int64_t writerIndex = 0; writerIndex < writerCount; ++writerIndex) {
            writers.emplace_back([&, writerIndex] {
                while (!started.load(std::memory_order_acquire) && running.load(std::memory_order_relaxed))
                    std::this_thread::yield();

                uint64_t writes = 0;
                uint64_t failures = 0;
                uint64_t sequence = static_cast<uint64_t>(writerIndex + 1);
                while (running.load(std::memory_order_relaxed)) {
                    const WalRecord record{
                        sequence,
                        static_cast<uint32_t>(sequence),
                        static_cast<uint16_t>(sequence),
                        static_cast<uint8_t>(sequence)
                    };
                    if (wal.Write(reinterpret_cast<const uint8_t*>(&record), sizeof(record), WAL_RECORD_TYPE) != 0)
                        ++writes;
                    else
                        ++failures;
                    sequence += static_cast<uint64_t>(writerCount);
                }

                successfulWrites[static_cast<size_t>(writerIndex)] = writes;
                failedWrites[static_cast<size_t>(writerIndex)] = failures;
            });
        }

        uint64_t fsyncAttempts = 0;
        const auto fsyncInterval = std::chrono::milliseconds(fsyncIntervalMs);

        started.store(true, std::memory_order_release);
        for (auto _ : state)  // MinTime 초과할 때 까지 반복
        { 
            std::this_thread::sleep_for(fsyncInterval);
            wal.Fsync();
            ++fsyncAttempts;
        }

        running.store(false, std::memory_order_relaxed);
        for (auto& writer : writers)
            writer.join();
        wal.Fsync();

        const uint64_t writes = std::accumulate(successfulWrites.begin(), successfulWrites.end(), uint64_t{0});
        const uint64_t failures = std::accumulate(failedWrites.begin(), failedWrites.end(), uint64_t{0});

        state.SetBytesProcessed(static_cast<int64_t>(writes * (sizeof(Base::WALHeader) + sizeof(WalRecord))));
        state.counters["fsync_attempts"] = static_cast<double>(fsyncAttempts);
        state.counters["write_failures"] = static_cast<double>(failures);
    }

    void WalArguments(benchmark::Benchmark* benchmark) {
        for (const int64_t fsyncIntervalMs : {10, 20, 25, 30, 50, 100}) {
            for (const int64_t writerCount : {1, 3})
                benchmark->Args({fsyncIntervalMs, writerCount});
        }
        benchmark->Args({ 25, 8 }); // lock contention stress
    }
}
BENCHMARK(BM_WalFsyncInterval)
->Apply(WalArguments)
->ArgNames({ "fsync_ms", "writers" })
->UseRealTime()
->Unit(benchmark::kMillisecond)
->MinTime(1.0)
->Repetitions(100)  // 반복
->ComputeStatistics("p01", P01)
->ComputeStatistics("p05", P05)
->DisplayAggregatesOnly(true); // 콘솔에 집계만 출력
