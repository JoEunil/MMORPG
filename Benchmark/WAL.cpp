#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <BaseLib/WAL.h>

namespace {
#pragma pack(push, 1)
    struct WalRecord {
        uint64_t sequence;
        uint32_t value32;
        uint16_t value16;
        uint8_t value8;
    };
#pragma pack(pop)
    static_assert(sizeof(WalRecord) == 15);

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

    void RunWalFsyncInterval(benchmark::State& state) {
        const int64_t fsyncIntervalMs = state.range(0);
        const int64_t writerCount = state.range(1);
        const int64_t payloadBytes = state.range(2);
        if (fsyncIntervalMs <= 0 || writerCount <= 0 || payloadBytes < static_cast<int64_t>(sizeof(WalRecord))
            || payloadBytes > static_cast<int64_t>(UINT16_MAX - sizeof(Base::WALHeader))) {
            state.SkipWithError("invalid fsync interval, writer count, or payload size");
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
                std::vector<uint8_t> payload(static_cast<size_t>(payloadBytes), 0x5a);
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
                    std::memcpy(payload.data(), &record, sizeof(record));
                    if (wal.Write(payload.data(), payload.size(), WAL_RECORD_TYPE) != 0) {
                        ++writes;
                    }
                    else {
                        ++failures;
                        running.store(false, std::memory_order_relaxed);
                    }
                    sequence += static_cast<uint64_t>(writerCount);
                }

                successfulWrites[static_cast<size_t>(writerIndex)] = writes;
                failedWrites[static_cast<size_t>(writerIndex)] = failures;
            });
        }

        uint64_t fsyncAttempts = 0;
        uint64_t fsyncFailures = 0;
        const auto fsyncInterval = std::chrono::milliseconds(fsyncIntervalMs);

        const auto writeStart = std::chrono::steady_clock::now();
        started.store(true, std::memory_order_release);
        for (auto _ : state)  // MinTime 초과할 때 까지 반복
        { 
            std::this_thread::sleep_for(fsyncInterval);
            const bool synced = wal.Fsync();
            ++fsyncAttempts;
            if (!synced) {
                ++fsyncFailures;
                running.store(false, std::memory_order_relaxed);
            }
            if (!running.load(std::memory_order_relaxed))
                break;
        }

        running.store(false, std::memory_order_relaxed);
        for (auto& writer : writers)
            writer.join();
        const auto writeEnd = std::chrono::steady_clock::now();
        const bool finalSynced = wal.Fsync();
        if (!finalSynced)
            ++fsyncFailures;
        const auto syncEnd = std::chrono::steady_clock::now();

        const uint64_t writes = std::accumulate(successfulWrites.begin(), successfulWrites.end(), uint64_t{0});
        const uint64_t failures = std::accumulate(failedWrites.begin(), failedWrites.end(), uint64_t{0});
        const double writeSeconds = std::chrono::duration<double>(writeEnd - writeStart).count();
        const double syncSeconds = std::chrono::duration<double>(syncEnd - writeStart).count();

        state.counters["successful_writes"] = static_cast<double>(writes);
        state.counters["write_ops_per_second"] = static_cast<double>(writes) / writeSeconds;
        state.counters["write_ops_per_second_including_final_fsync"] = static_cast<double>(writes) / syncSeconds;
        state.counters["write_wall_seconds"] = writeSeconds;
        state.counters["wall_seconds_including_final_fsync"] = syncSeconds;
        state.counters["payload_bytes"] = static_cast<double>(payloadBytes);
        state.counters["writers"] = static_cast<double>(writerCount);
        state.counters["fsync_failures"] = static_cast<double>(fsyncFailures);
        state.counters["final_fsync_success"] = finalSynced ? 1.0 : 0.0;
        state.SetBytesProcessed(static_cast<int64_t>(writes * (sizeof(Base::WALHeader) + payloadBytes)));
        state.counters["fsync_attempts"] = static_cast<double>(fsyncAttempts);
        state.counters["write_failures"] = static_cast<double>(failures);
        if (failures != 0 || fsyncFailures != 0)
            state.SkipWithError("WAL write or fsync failed");
    }

    void BM_WalFsyncInterval(benchmark::State& state) {
        RunWalFsyncInterval(state);
    }

    void WalArguments(benchmark::Benchmark* benchmark) {
        for (const int64_t fsyncIntervalMs : {10, 20, 25, 30, 50, 100}) {
            for (const int64_t writerCount : {1, 3})
                benchmark->Args({fsyncIntervalMs, writerCount, sizeof(WalRecord)});
        }
        benchmark->Args({25, 8, sizeof(WalRecord)}); // lock contention stress
        for (const int64_t fsyncIntervalMs : {10, 20, 25, 30, 50, 100})
            benchmark->Args({fsyncIntervalMs, 3, 512});
    }
}
BENCHMARK(BM_WalFsyncInterval)
->Apply(WalArguments)
->ArgNames({"fsync_ms", "writers", "payload_bytes"})
->UseRealTime()
->Unit(benchmark::kMillisecond)
->Repetitions(100)
->ReportAggregatesOnly(false);
