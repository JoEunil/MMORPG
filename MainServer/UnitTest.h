#pragma once

#include <iostream>
#include <cassert>

#include <BaseLib/RingQueue.h>
#include <BaseLib/LockFreeQueue.h>

inline static int TestRingQueue() {
    Base::RingQueue<int, 8> q; // 8 = 2의 거듭제곱
    assert(q.empty());
    // push 테스트
    for (int i = 1; i <= 7; ++i) { // one-slot-empty 전략
        q.push(i);
        std::cout << "push: " << i << ", size: " << q.size() << "\n";
    }

    assert(q.full());

    // pop 테스트
    while (!q.empty()) {
        int val = q.pop();
        std::cout << "pop: " << val << ", size: " << q.size() << "\n";
    }
    assert(q.empty());
    // wrap-around 테스트
    for (int i = 10; i <= 13; ++i) q.push(i); 
    for (int i = 0; i < 2; ++i) q.pop(); 
    for (int i = 20; i <= 22; ++i) q.push(i);
    while (!q.empty()) {
        std::cout << q.pop() << " ";
    }
    std::cout << "\n";

    std::cout << "RingQueue basic test passed!\n";
    return 0;
}


#pragma once

#include <iostream>
#include <cassert>

#include <BaseLib/RingQueue.h>
#include <BaseLib/LockFreeQueue.h>

inline static int TestRingQueue() {
    Base::RingQueue<int, 8> q; // 8 = 2의 거듭제곱
    assert(q.empty());
    // push 테스트
    for (int i = 1; i <= 7; ++i) { // one-slot-empty 전략
        q.push(i);
        std::cout << "push: " << i << ", size: " << q.size() << "\n";
    }

    assert(q.full());

    // pop 테스트
    while (!q.empty()) {
        int val = q.pop();
        std::cout << "pop: " << val << ", size: " << q.size() << "\n";
    }
    assert(q.empty());
    // wrap-around 테스트
    for (int i = 10; i <= 13; ++i) q.push(i); 
    for (int i = 0; i < 2; ++i) q.pop(); 
    for (int i = 20; i <= 22; ++i) q.push(i);
    while (!q.empty()) {
        std::cout << q.pop() << " ";
    }
    std::cout << "\n";

    std::cout << "RingQueue basic test passed!\n";
    return 0;
}



constexpr int QUEUE_SIZE = 1024;
constexpr int NUM_PRODUCERS = 4;
constexpr int NUM_CONSUMERS = 4;
constexpr int OPS_PER_THREAD = 10000;

void Producer(Base::LockFreeQueue<int, QUEUE_SIZE>& q, std::atomic<int>& counter, int id) {
    for (int j = 0; j < OPS_PER_THREAD; ++j) {
        int val = id * OPS_PER_THREAD + j;
        while (!q.push(val)) {
            std::this_thread::yield;
        }
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

void Consumer(Base::LockFreeQueue<int, QUEUE_SIZE>& q, std::atomic<int>& counter) {
    int val;
    while (true) {
        if (q.pop(val)) {
            counter.fetch_sub(1, std::memory_order_relaxed);
        }
        else {
            if (counter.load(std::memory_order_relaxed) == 0) break;
            std::this_thread::yield;
        }
    }
}

inline static int LockFreeQueueTest() {
    Base::LockFreeQueue<int, QUEUE_SIZE> q;
    std::atomic<int> counter{ 0 };

    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i)
        producers.emplace_back(Producer, std::ref(q), std::ref(counter), i);
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i)
        consumers.emplace_back(Consumer, std::ref(q), std::ref(counter));

    for (auto& t : producers) t.join();
    //0 ~ 40000까지 push, back-off 정책은 yield
    for (auto& t : consumers) t.join();
    //0 ~ 40000까지 pop, back-off 정책은 yield, counter가 0이 되는 순간 종료.

    std::cout << "All threads finished. Counter: " << counter.load() << "\n";
    assert(counter.load() == 0);
    std::cout << "LockFreeQueue multi-threaded test passed!\n";
    return 0;
}