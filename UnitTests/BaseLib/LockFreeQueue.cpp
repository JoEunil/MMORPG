#include <gtest/gtest.h>
#include <thread>
#include <set>
#include <mutex>
#include <memory>

#include <BaseLib/LockFreeQueue.h>

// value / unique_ptr / shared_ptr 를 모두 통합된 Base::LockFreeQueue 하나로 검증한다.
// (기존 LockFreeQueueSP / LockFreeQueueUP 분기는 통합됨)

constexpr int QUEUE_SIZE1 = 1024;
constexpr int QUEUE_SIZE2 = 1024;
constexpr int THREADS = 4;
constexpr int REQUEST_PER_THREAD = 100;

struct Dummy {
    Dummy() = default;
    Dummy(int val) : v(val) {};
    int v = 0;
};

// value 타입
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

// unique_ptr (move-only)
// consume-on-success: push 성공 시 원본이 move되어 비고, 실패(full) 시 원본은 유지된다(유실 없음).
TEST(LockFreeQueueUPTest, SingleThreadPushPop) {
    Base::LockFreeQueue<std::unique_ptr<Dummy>, QUEUE_SIZE1> q;
    auto in = std::make_unique<Dummy>(Dummy{2});
    EXPECT_TRUE(q.push(in));
    EXPECT_EQ(in, nullptr);            // 성공 시 move되어 원본은 비워짐
    std::unique_ptr<Dummy> out;
    ASSERT_TRUE(q.pop(out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->v, 2);
}

TEST(LockFreeQueueUPTest, PopFromEmptyFails) {
    Base::LockFreeQueue<std::unique_ptr<Dummy>, QUEUE_SIZE1> q;
    std::unique_ptr<Dummy> out;
    EXPECT_FALSE(q.pop(out));
    EXPECT_EQ(out, nullptr);
}

TEST(LockFreeQueueUPTest, PushToFullFails) {
    Base::LockFreeQueue<std::unique_ptr<Dummy>, QUEUE_SIZE1> q;
    for (int i = 0; i < QUEUE_SIZE1; ++i) {
        auto in = std::make_unique<Dummy>(Dummy{ i });
        EXPECT_TRUE(q.push(in));
    }
    auto overflow = std::make_unique<Dummy>(Dummy{ QUEUE_SIZE1 });
    EXPECT_FALSE(q.push(overflow));    // 꽉 찬 큐 push는 false
    EXPECT_NE(overflow, nullptr);      // 실패 시 원본 유지 → move-only 유실 없음
}

TEST(LockFreeQueueUPTest, MutiThreadSafePushRace) {
    Base::LockFreeQueue<std::unique_ptr<Dummy>, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> pushed;
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&, i]() {
            for (int j = 0; j < REQUEST_PER_THREAD; j++) {
                auto in = std::make_unique<Dummy>(Dummy{ i * REQUEST_PER_THREAD + j });
                if (q.push(in)) {
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
    while (q.pop(out)) {
        pushed.insert(out->v);
    }
    EXPECT_EQ(pushed.size(), THREADS * REQUEST_PER_THREAD);
}

TEST(LockFreeQueueUPTest, MutiThreadSafePopRace) {
    Base::LockFreeQueue<std::unique_ptr<Dummy>, QUEUE_SIZE2> q;
    std::vector<std::thread> threads;
    std::atomic<int> successCnt = 0;
    std::set<int> popped;
    std::mutex setMutex;

    for (int i = 0; i < THREADS * REQUEST_PER_THREAD; i++)
    {
        auto in = std::make_unique<Dummy>(Dummy{ i });
        q.push(in);
    }
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread([&]() {
            std::unique_ptr<Dummy> out;
            std::set<int> local;
            for (int j = 0; j < REQUEST_PER_THREAD; j++)
            {
                if (q.pop(out)) {
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
// move-out으로 슬롯이 자동 비워지므로 pop 후 refcount는 1 (별도 clear 불필요).
TEST(LockFreeQueueSPTest, SingleThreadPushPop) {
    Base::LockFreeQueue<std::shared_ptr<Dummy>, QUEUE_SIZE1> q;
    EXPECT_TRUE(q.push(std::make_shared<Dummy>(Dummy{2})));
    std::shared_ptr<Dummy> out;
    ASSERT_TRUE(q.pop(out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->v, 2);
    EXPECT_EQ(1, out.use_count());
}

TEST(LockFreeQueueSPTest, PopFromEmptyFails) {
    Base::LockFreeQueue<std::shared_ptr<Dummy>, QUEUE_SIZE1> q;
    std::shared_ptr<Dummy> out;
    EXPECT_FALSE(q.pop(out));
    EXPECT_EQ(out, nullptr);
}

TEST(LockFreeQueueSPTest, PushToFullFails) {
    Base::LockFreeQueue<std::shared_ptr<Dummy>, QUEUE_SIZE1> q;
    for (int i = 0; i < QUEUE_SIZE1; ++i) {
        EXPECT_TRUE(q.push(std::make_shared<Dummy>(Dummy{ i })));
    }
    EXPECT_FALSE(q.push(std::make_shared<Dummy>(Dummy{QUEUE_SIZE1})));   // 꽉 찬 큐 push는 false
}

TEST(LockFreeQueueSPTest, MutiThreadSafePushRace) {
    Base::LockFreeQueue<std::shared_ptr<Dummy>, QUEUE_SIZE2> q;
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
    while (q.pop(out)) {
        EXPECT_EQ(1, out.use_count());
        pushed.insert(out->v);
    }
    EXPECT_EQ(pushed.size(), THREADS * REQUEST_PER_THREAD);
}

TEST(LockFreeQueueSPTest, MutiThreadSafePopRace) {
    Base::LockFreeQueue<std::shared_ptr<Dummy>, QUEUE_SIZE2> q;
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
                if (q.pop(out)) {
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
