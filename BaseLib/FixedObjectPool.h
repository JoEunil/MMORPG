#pragma once

#include <vector>
#include <memory>
#include <new>
#include "SpinLockGuard.h"


namespace Base {
    template<typename T, size_t Size>
    class FixedObjectPool {
        struct alignas(T) Slot { unsigned char data[sizeof(T)]; };  // 생성자 호출 없이 메모리 예약하기 위함.
        std::unique_ptr<Slot[]> m_storage;
        std::vector<T*> freeList;
        alignas(std::hardware_destructive_interference_size) std::atomic_flag lock;
    public:
        template<typename... Args>
        explicit FixedObjectPool(const Args&... args)
            : m_storage(std::make_unique<Slot[]>(Size)) {
            freeList.reserve(Size);
            for (size_t i = 0; i < Size; i++)
                freeList.push_back(::new (&m_storage[i]) T(args...)); // 주소에 생성자 호출 placement new
        }

        ~FixedObjectPool() {                  
            for (size_t i = 0; i < Size; i++)
                reinterpret_cast<T*>(&m_storage[i])->~T(); // 소멸자 쓰려면 Slot을 T로 캐스팅 필요.
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