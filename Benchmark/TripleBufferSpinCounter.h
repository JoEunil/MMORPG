#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <new>

namespace BenchmarkSupport {
    template <typename T>
    struct SpinCountBufferReader;

    template <typename T>
    class TripleBufferSpinCounter {
        alignas(std::hardware_destructive_interference_size) T* back1 = nullptr;
        alignas(std::hardware_destructive_interference_size) T* back2 = nullptr;
        alignas(std::hardware_destructive_interference_size) std::atomic<uint16_t> flag = 0;
        char padding[std::hardware_destructive_interference_size - sizeof(uint16_t)];

    public:
        ~TripleBufferSpinCounter() {
            delete back1;
            delete back2;
        }

        void Init(T* b1, T* b2) {
            back1 = b1;
            back2 = b2;
        }

        uint32_t Write(T*& write) {
            uint32_t spinCount = 0;
            while (true) {
                const uint16_t oldFlag = flag.load(std::memory_order_relaxed);
                uint16_t expected = oldFlag & (0x8000 - 1);
                const uint16_t desired = oldFlag | 0x8000;
                if (flag.compare_exchange_weak(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    break;
                }
                ++spinCount;
            }

            std::swap(back1, write);

            while (true) {
                uint16_t oldFlag = flag.load(std::memory_order_relaxed);
                const uint16_t desired = oldFlag & (0x4000 - 1);
                if (flag.compare_exchange_weak(oldFlag, desired,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    break;
                }
            }

            return spinCount;
        }

        SpinCountBufferReader<T> Read() {
            uint16_t newFlag;
            while (true) {
                uint16_t oldFlag = flag.load(std::memory_order_relaxed);
                if (oldFlag == 0)
                    newFlag = 0x8000;
                else if (oldFlag & 0x8000)
                    continue;
                else
                    newFlag = oldFlag + 1;

                if (flag.compare_exchange_weak(oldFlag, newFlag,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    break;
                }
            }

            if (newFlag == 0x8000) {
                std::swap(back1, back2);
                flag.store(0x4000 + 1, std::memory_order_release);
            }

            return SpinCountBufferReader<T>(back2, this);
        }

        void ReadDone() {
            flag.fetch_sub(1, std::memory_order_release);
        }
    };

    template <typename T>
    struct SpinCountBufferReader {
        T* data = nullptr;
        TripleBufferSpinCounter<T>* owner = nullptr;

        SpinCountBufferReader(T* ptr, TripleBufferSpinCounter<T>* buffer)
            : data(ptr), owner(buffer) {
        }

        ~SpinCountBufferReader() {
            if (owner != nullptr)
                owner->ReadDone();
        }

        SpinCountBufferReader(const SpinCountBufferReader&) = delete;
        SpinCountBufferReader& operator=(const SpinCountBufferReader&) = delete;

        SpinCountBufferReader(SpinCountBufferReader&& other) noexcept
            : data(other.data), owner(other.owner) {
            other.data = nullptr;
            other.owner = nullptr;
        }

        SpinCountBufferReader& operator=(SpinCountBufferReader&&) = delete;
    };
}
