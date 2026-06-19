#include <gtest/gtest.h>
#include <thread>

#include <BaseLib/LockFreeQueue.h>

constexpr int QUEUE_SIZE = 1024;
constexpr int NUM_PRODUCERS = 4;
constexpr int NUM_CONSUMERS = 4;
constexpr int OPS_PER_THREAD = 10000;
constexpr int TOTAL = NUM_PRODUCERS * OPS_PER_THREAD;

void Producer(Base::LockFreeQueue<int, QUEUE_SIZE>& q, std::atomic<int>& counter, int id) {
    for (int j = 0; j < OPS_PER_THREAD; ++j) {
        int val = id * OPS_PER_THREAD + j;
        while (!q.push(val)) {
            std::this_thread::yield();
        }
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

void Consumer(Base::LockFreeQueue<int, QUEUE_SIZE>& q, std::atomic<int>& counter, std::vector<std::atomic<int>>& seen) {
    int val;
    while (counter.load(std::memory_order_relaxed) < TOTAL) {
        if (q.pop(val)) {
			seen[val].fetch_add(1, std::memory_order_relaxed);
            counter.fetch_add(1, std::memory_order_relaxed);
        }
        else {
            std::this_thread::yield();
        }
    }
}

TEST(LockFreeQueueTest, SingleThreadPushPop) {
    Base::LockFreeQueue<int, 8> q;
    int val;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.pop(val));
    EXPECT_EQ(val, 1);
}

TEST(LockFreeQueueTest, PopFromEmptyFails) {
    Base::LockFreeQueue<int, 8> q;
    int val;
    EXPECT_FALSE(q.pop(val));   // 빈 큐 pop은 false
}

TEST(LockFreeQueueTest, PushToFullFails) {
    Base::LockFreeQueue<int, 4> q;
    for (int i = 0 ; i < 4; ++i) {
        EXPECT_TRUE(q.push(i));
	}
	EXPECT_FALSE(q.push(4));   // 꽉 찬 큐 push는 false
}

TEST(LockFreeQueueTest, ConcurrentNoLossNoDuplication) {
    // 4 producer × 4 consumer, 유실 0 + 중복 0 검증
    Base::LockFreeQueue<int, QUEUE_SIZE> q;
    alignas(64) std::atomic<int> produced{0};
    alignas(64) std::atomic<int> consumed{0};

    std::vector<std::atomic<int>> seen(TOTAL); 

    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i)
        producers.emplace_back(Producer, std::ref(q), std::ref(produced), i);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i)
        consumers.emplace_back(Consumer, std::ref(q), std::ref(consumed), std::ref(seen));

    for (auto& t : producers) t.join();
    //0 ~ 40000까지 push, back-off 정책은 yield
    for (auto& t : consumers) t.join();
    //0 ~ 40000까지 pop, back-off 정책은 yield, counter가 0이 되는 순간 종료.

	EXPECT_EQ(produced.load(), TOTAL); 
	EXPECT_EQ(consumed.load(), TOTAL); 
    
    for (int i = 0; i < TOTAL; ++i) {
        EXPECT_EQ(seen[i].load(), 1) << "Value " << i << " was seen " << seen[i].load() << " times";
        if (i != 1)
            break;
	}
}