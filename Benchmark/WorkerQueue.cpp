#include <benchmark/benchmark.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <latch>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <utility>
#include <BaseLib/LockFreeQueue.h>
#include <BaseLib/BlockingQueue.h>
#include <BaseLib/RingQueue.h>

namespace {
    constexpr uint64_t TotalItems = 1048576;
    using Item = uint64_t;
    template<size_t Capacity>
    struct PollQueue {
        Base::LockFreeQueue<Item, Capacity> queue;
        bool push(Item& x) { return queue.push(x); }
        bool pop(Item& x) { return queue.pop(x); }
        void Stop() {}
    };
    template<size_t Capacity>
    struct SemaphoreQueue {
        Base::BlockingQueue<Item, Capacity, true> queue;
        bool push(Item& x) { return queue.push(x); }
        bool pop(Item& x) { return queue.pop(x); }
        void Stop() { queue.Stop(); }
    };
    // Same bounded RingQueue + mutex/predicate/CV policy as the cache workers.
    template<size_t Capacity>
    struct MutexCVQueue {
        Base::RingQueue<Item, Capacity> queue;
        std::mutex mutex;
        std::condition_variable cv;
        bool closed = false;
        bool push(Item& x) {
            { std::lock_guard lock(mutex); if (!queue.push(x)) return false; }
            cv.notify_one();
            return true;
        }
        bool pop(Item& x) {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&] { return closed || !queue.empty(); });
            return queue.pop(x);
        }
        void Stop() {
            { std::lock_guard lock(mutex); closed = true; }
            cv.notify_all();
        }
    };
    template<class Queue, bool Blocking, bool SleepOnEmpty>
    void Run(benchmark::State& state) {
        const size_t producerCount = static_cast<size_t>(state.range(0));
        const size_t consumerCount = static_cast<size_t>(state.range(1));
        const uint64_t perProducer = TotalItems / producerCount;
        uint64_t completedItems = 0, pushFailures = 0;
        for (auto _ : state) {
            Queue queue;
            std::atomic<bool> producersDone{false};
            std::latch ready(producerCount + consumerCount), startGate(1);
            std::vector<uint64_t> consumed(consumerCount), retries(producerCount);
            std::vector<std::thread> producers, consumers;
            for (size_t c = 0; c < consumerCount; ++c) consumers.emplace_back([&, c] {
                ready.count_down();
                startGate.wait();
                Item item;
                uint64_t count = 0;
                while (true) {
                    if (queue.pop(item)) {
                        benchmark::DoNotOptimize(item);
                        ++count;
                    } else {
                        if constexpr (Blocking) break;
                        else {
                            if (producersDone.load(std::memory_order_acquire)) {
                                // Recheck after acquire: a producer may have published after the failed pop.
                                if (queue.pop(item)) { benchmark::DoNotOptimize(item); ++count; continue; }
                                break;
                            }
                            if constexpr (SleepOnEmpty)
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            else
                                std::this_thread::yield();
                        }
                    }
                }
                consumed[c] = count;
            });
            for (size_t p = 0; p < producerCount; ++p) producers.emplace_back([&, p] {
                ready.count_down();
                startGate.wait();
                uint64_t failures = 0;
                for (uint64_t i = 1; i <= perProducer; ++i) {
                    Item item = p * perProducer + i;
                    while (!queue.push(item)) {
                        ++failures;
                        std::this_thread::yield();
                    }
                }
                retries[p] = failures;
            });
            ready.wait();
            const auto start = std::chrono::steady_clock::now();
            startGate.count_down();
            for (auto& t : producers) t.join();
            producersDone.store(true, std::memory_order_release);
            queue.Stop(); // all producers have stopped; drain accepted work
            for (auto& t : consumers) t.join();
            const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            state.SetIterationTime(seconds);
            uint64_t count = 0;
            for (auto n : consumed) count += n;
            if (count != TotalItems) {
                state.SkipWithError("consumed count mismatch"); break;
            }
            completedItems += count;
            for (auto n : retries) pushFailures += n;
        }
        state.SetItemsProcessed(completedItems);
        state.counters["push_failures/item"] = completedItems
            ? static_cast<double>(pushFailures) / completedItems : 0;
    }
    template<template<size_t> class Queue, bool Blocking, bool SleepOnEmpty = false>
    void RunCapacity(benchmark::State& state) {
        switch (state.range(2)) {
        case 1024: Run<Queue<1024>, Blocking, SleepOnEmpty>(state); break;
        case 16384: Run<Queue<16384>, Blocking, SleepOnEmpty>(state); break;
        default: state.SkipWithError("unsupported queue capacity"); break;
        }
    }
    void BM_WorkerQueue_Poll(benchmark::State& s) { RunCapacity<PollQueue, false>(s); }
    void BM_WorkerQueue_PollSleep(benchmark::State& s) { RunCapacity<PollQueue, false, true>(s); }
    void BM_WorkerQueue_Semaphore(benchmark::State& s) { RunCapacity<SemaphoreQueue, true>(s); }
    void BM_WorkerQueue_MutexCV(benchmark::State& s) { RunCapacity<MutexCVQueue, true>(s); }
    void QueueArgs(benchmark::Benchmark* b) {
        b->ArgNames({"producers", "consumers", "capacity"});
        for (auto [p, c] : {std::pair{1, 1}, {4, 1}, {1, 4}, {4, 4}})
            for (int capacity : {1024, 16384})
                b->Args({p, c, capacity});
    }
}
BENCHMARK(BM_WorkerQueue_Poll)->Apply(QueueArgs)
    ->UseManualTime()->MinTime(10.0)->Repetitions(1)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_WorkerQueue_PollSleep)->Apply(QueueArgs)
    ->UseManualTime()->MinTime(10.0)->Repetitions(1)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_WorkerQueue_Semaphore)->Apply(QueueArgs)
    ->UseManualTime()->MinTime(10.0)->Repetitions(1)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_WorkerQueue_MutexCV)->Apply(QueueArgs)
    ->UseManualTime()->MinTime(10.0)->Repetitions(1)->Unit(benchmark::kMillisecond);
