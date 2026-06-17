#pragma once

#include <vector>
#include <memory>
#include <new>
#include "SpinLockGuard.h"


namespace Base {
    template<typename T, size_t Size>
    class FixedObjectPool {
        std::unique_ptr<T[]> pool; // 힙 할당
        std::vector<T*> freeList;
        alignas(std::hardware_destructive_interference_size) std::atomic_flag lock;
    public:
        FixedObjectPool() : pool(std::make_unique<T[]>(Size)) {
            freeList.reserve(Size);
            for (size_t i = 0; i < Size; i++)
                freeList.push_back(&pool[i]);
        }
        T* Allocate() {
            SpinLockGuard lockGuard(lock);
            if (freeList.empty())
                return nullptr;
            auto ptr = freeList.back();
            freeList.pop_back();
            return ptr;
        }

        void Deallocate(T* ptr) {
            SpinLockGuard lockGuard(lock);
            freeList.push_back(ptr);
        }
    };
}