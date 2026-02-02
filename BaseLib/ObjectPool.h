#pragma once
#include <mutex>
#include <cstdint>
#include <vector>


namespace Base {
    template <typename T>
    class ObjectPool {
        std::vector<T*> objects;
        std::mutex mutex;
        const size_t target, max, min;

        void Adjust() {
            if (objects.size() >= max) {
                Decrease(objects.size());
            }
            if (objects.size() <= min) {
                Increase(objects.size());
            }
        }
        void Increase(uint16_t currentSize) {
            while (currentSize++ < target) {
                objects.emplace_back(new T);
            }
        }
        void Decrease(uint16_t currentSize) {
            while (currentSize-- > target) {
                delete objects.back();
                objects.pop_back();
            }
        }
    public:
        ObjectPool(size_t target, size_t min, size_t max)
        {
            objects.reserve(max);
            for (int i = 0; i < target; i++) 
            {
                objects.emplace_back(new T);
            }
        }
        ~ObjectPool()
        {
            for (T* object : objects)
                delete object;
        }
        T* Acquire() {
            std::lock_guard<std::mutex> lock(mutex);
            Adjust();
            T* res = objects.back();
            objects.pop_back();
            return res;
        }
        void Return(T* data) {
            std::lock_guard<std::mutex> lock(mutex);
            objects.push_back(data);
            Adjust();
        }
    };
}
