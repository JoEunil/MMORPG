#include <gtest/gtest.h>
#include <thread>
#include <set>
#include <mutex>

#include <BaseLib/LockFreeQueue.h>
#include <BaseLib/LockFreeQueueSP.h>
#include <BaseLib/LockFreeQueueUP.h>


constexpr int QUEUE_SIZE1 = 1024;
constexpr int QUEUE_SIZE2 = 1024;
constexpr int THREADS = 4;
constexpr int REQUEST_PER_THREAD = 100;

struct Dummy {
    Dummy() = default;
    Dummy(int val) : v(val) {};
    int v = 0;
};

TEST(LockFreeQueueTest, SingleThreadPushPop) {
    Base::LockFreeQueue<Dummy, QUEUE_SIZE1> q;
    Dummy out;
    EXPECT_TRUE(q.push(Dummy{1}));
    EXPECT_TRUE(q.pop(out));
    EXPECT_EQ(out.v, 1);
}

TEST(LockFreeQueueTest, PopFromEmptyFails) {
    Base::LockFreeQueue<Dummy, QUEUE_SIZE1> q;
    Dummy out;
    EXPECT_FALSE(q.pop(out));   // 빈 큐 pop은 false
}

TEST(LockFreeQueueTest, PushToFullFails) {
    Base::LockFreeQueue<Dummy, QUEUE_SIZE1> q;
    for (int i = 0 ; i < QUEUE_SIZE1; ++i) {
        EXPECT_TRUE(q.push(Dummy{ i }));
	}
	EXPECT_FALSE(q.push(QUEUE_SIZE1));   // 꽉 찬 큐 push는 false
}

TEST(LockFreeQueueTest, MutiThreadSafePushRace) { 
    Base::LockFreeQueue<Dummy, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> pushed;
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&, i]() {
            for (int j = 0; j < REQUEST_PER_THREAD; j++) {
                if (q.push(Dummy{ i * REQUEST_PER_THREAD + j })) {
                    successCnt.fetch_add(1, std::memory_order_relaxed);
                }
            }
            } ));
    }
    for (int i = 0; i < THREADS; i++) {
        threads[i].join();
    }
    EXPECT_EQ(successCnt.load(std::memory_order_relaxed), THREADS * REQUEST_PER_THREAD);

    Dummy out;
    while (q.pop(out)) {
        pushed.insert(out.v);
    }
    EXPECT_EQ(pushed.size(), THREADS * REQUEST_PER_THREAD);
}

TEST(LockFreeQueueTest, MutiThreadSafePopRace) {
    Base::LockFreeQueue<Dummy, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> popped;
    std::mutex setMutex;
    
    for (int i = 0; i < THREADS * REQUEST_PER_THREAD; i++)
    {
        q.push(Dummy{i});
    }
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&]() {
            Dummy out;
            std::set<int> local;
            for (int j = 0; j < REQUEST_PER_THREAD; j++)
            {
                if (q.pop(out)) {
                    successCnt.fetch_add(1, std::memory_order_relaxed);
                    local.insert(out.v);
                }
            }
            std::lock_guard<std::mutex> lock(setMutex);
            popped.insert(local.begin(), local.end());
            }));
    }

    for (int i = 0; i < THREADS; i++) {
        threads[i].join();
    }
    EXPECT_EQ(successCnt.load(std::memory_order_relaxed), THREADS * REQUEST_PER_THREAD);
    EXPECT_EQ(popped.size(), THREADS * REQUEST_PER_THREAD);

}

// unique_ptr
// unique_ptr은 실패 처리 때문에 push 성공 시 nullptr, 실패 시 unique_ptr 반환
TEST(LockFreeQueueUPTest, SingleThreadPushPop) {
    Base::LockFreeQueueUP<std::unique_ptr<Dummy>, QUEUE_SIZE1> q;
    EXPECT_EQ(q.push(std::make_unique<Dummy>(Dummy{2})), nullptr);
    std::unique_ptr<Dummy> out = q.pop(); 
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->v, 2);
}

TEST(LockFreeQueueUPTest, PopFromEmptyFails) {
    Base::LockFreeQueueUP<std::unique_ptr<Dummy>, QUEUE_SIZE1> q;
    std::unique_ptr<Dummy> out = q.pop();
    EXPECT_EQ(out, nullptr);
}

TEST(LockFreeQueueUPTest, PushToFullFails) {
    Base::LockFreeQueueUP<std::unique_ptr<Dummy>, QUEUE_SIZE1> q;
    for (int i = 0; i < QUEUE_SIZE1; ++i) {
        EXPECT_EQ(q.push(std::make_unique<Dummy>(Dummy{ i })), nullptr);
    }
    EXPECT_NE(q.push(std::make_unique<Dummy>(Dummy{QUEUE_SIZE1})), nullptr);   // 꽉 찬 큐 push는 false
}

TEST(LockFreeQueueUPTest, MutiThreadSafePushRace) {
    Base::LockFreeQueueUP<std::unique_ptr<Dummy>, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> pushed;
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&, i]() {
            for (int j = 0; j < REQUEST_PER_THREAD; j++) {
                if (q.push(std::make_unique<Dummy>(Dummy{ i * REQUEST_PER_THREAD + j })) == nullptr) {
                    successCnt.fetch_add(1, std::memory_order_relaxed);
                }
            }
            }));
    }
    for (int i = 0; i < THREADS; i++) {
        threads[i].join();
    }
    EXPECT_EQ(successCnt.load(std::memory_order_relaxed), THREADS * REQUEST_PER_THREAD);

    std::unique_ptr<Dummy> out;
    while (out = q.pop()) {
        pushed.insert(out->v);
    }
    EXPECT_EQ(pushed.size(), THREADS * REQUEST_PER_THREAD);
}

TEST(LockFreeQueueUPTest, MutiThreadSafePopRace) {
    Base::LockFreeQueueUP<std::unique_ptr<Dummy>, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> popped;
    std::mutex setMutex;

    for (int i = 0; i < THREADS * REQUEST_PER_THREAD; i++)
    {
        q.push(std::make_unique<Dummy>(Dummy{ i }));
    }
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&]() {
            std::unique_ptr<Dummy> out;
            std::set<int> local;
            for (int j = 0; j < REQUEST_PER_THREAD; j++)
            {
                if (out = q.pop() ) {
                    successCnt.fetch_add(1, std::memory_order_relaxed);
                    local.insert(out->v);
                }
            }
            std::lock_guard<std::mutex> lock(setMutex);
            popped.insert(local.begin(), local.end());
            }));
    }

    for (int i = 0; i < THREADS; i++) {
        threads[i].join();
    }
    EXPECT_EQ(successCnt.load(std::memory_order_relaxed), THREADS * REQUEST_PER_THREAD);
    EXPECT_EQ(popped.size(), THREADS * REQUEST_PER_THREAD);

}

// shared_ptr
TEST(LockFreeQueueSPTest, SingleThreadPushPop) {
    Base::LockFreeQueueSP<std::shared_ptr<Dummy>, QUEUE_SIZE1> q;
    EXPECT_TRUE(q.push(std::make_shared<Dummy>(Dummy{2})));
    std::shared_ptr<Dummy> out = q.pop();
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->v, 2);
    EXPECT_EQ(1, out.use_count());
}

TEST(LockFreeQueueSPTest, PopFromEmptyFails) {
    Base::LockFreeQueueSP<std::shared_ptr<Dummy>, QUEUE_SIZE1> q;
    std::shared_ptr<Dummy> out = q.pop();
    EXPECT_EQ(out, nullptr);
}

TEST(LockFreeQueueSPTest, PushToFullFails) {
    Base::LockFreeQueueSP<std::shared_ptr<Dummy>, QUEUE_SIZE1> q;
    for (int i = 0; i < QUEUE_SIZE1; ++i) {
        EXPECT_TRUE(q.push(std::make_shared<Dummy>(Dummy{ i })));
    }
    EXPECT_FALSE(q.push(std::make_shared<Dummy>(Dummy{QUEUE_SIZE1})));   // 꽉 찬 큐 push는 false
}

TEST(LockFreeQueueSPTest, MutiThreadSafePushRace) {
    Base::LockFreeQueueSP<std::shared_ptr<Dummy>, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> pushed;
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&, i]() {
            for (int j = 0; j < REQUEST_PER_THREAD; j++) {
                if (q.push(std::make_shared<Dummy>(Dummy{ i * REQUEST_PER_THREAD + j }))) {
                    successCnt.fetch_add(1, std::memory_order_relaxed);
                }
            }
            }));
    }
    for (int i = 0; i < THREADS; i++) {
        threads[i].join();
    }
    EXPECT_EQ(successCnt.load(std::memory_order_relaxed), THREADS * REQUEST_PER_THREAD);

    std::shared_ptr<Dummy> out;
    while (out = q.pop()) {
        EXPECT_EQ(1, out.use_count());
        pushed.insert(out->v);
    }
    EXPECT_EQ(pushed.size(), THREADS * REQUEST_PER_THREAD);
}

TEST(LockFreeQueueSPTest, MutiThreadSafePopRace) {
    Base::LockFreeQueueSP<std::shared_ptr<Dummy>, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> popped;
    std::mutex setMutex;

    for (int i = 0; i < THREADS * REQUEST_PER_THREAD; i++)
    {
        q.push(std::make_shared<Dummy>(Dummy{ i }));
    }
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&]() {
            std::shared_ptr<Dummy> out;
            std::set<int> local;
            for (int j = 0; j < REQUEST_PER_THREAD; j++)
            {
                if (out = q.pop()) {
                    successCnt.fetch_add(1, std::memory_order_relaxed);
                    local.insert(out->v);
                }
            }
            std::lock_guard<std::mutex> lock(setMutex);
            popped.insert(local.begin(), local.end());
            }));
    }

    for (int i = 0; i < THREADS; i++) {
        threads[i].join();
    }
    EXPECT_EQ(successCnt.load(std::memory_order_relaxed), THREADS * REQUEST_PER_THREAD);
    EXPECT_EQ(popped.size(), THREADS * REQUEST_PER_THREAD);

}