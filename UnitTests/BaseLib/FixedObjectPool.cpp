#include <gtest/gtest.h>
#include <set>
#include <thread>
#include <mutex>
#include <BaseLib/FixedObjectPool.h>

struct Dummy { int v = 0; };

TEST(FixedObjectPoolTest, AllocateReturnsValid) {
    Base::FixedObjectPool<Dummy, 4> pool;
    Dummy* p = pool.Allocate();
    ASSERT_NE(p, nullptr);        // 역참조 전 ASSERT
    p->v = 42;                    // 안전하게 접근
    EXPECT_EQ(p->v, 42);
}

TEST(FixedObjectPoolTest, ExhaustionReturnsNullptr) {
    Base::FixedObjectPool<Dummy, 4> pool;
    for (int i = 0; i < 4; ++i)
        EXPECT_NE(pool.Allocate(), nullptr) << "i=" << i;   // 4개까진 성공
    EXPECT_EQ(pool.Allocate(), nullptr);   // 5번째는 고갈 → nullptr
}

TEST(FixedObjectPoolTest, DeallocateReturnsToPool) {
    Base::FixedObjectPool<Dummy, 2> pool;
    Dummy* a = pool.Allocate();
    Dummy* b = pool.Allocate();
    EXPECT_EQ(pool.Allocate(), nullptr);   // 고갈 확인
    pool.Deallocate(a);                    // 하나 반납
    EXPECT_NE(pool.Allocate(), nullptr);   // 다시 할당 가능
}

TEST(FixedObjectPoolTest, DeallocateReusesSameSlot) {
    Base::FixedObjectPool<Dummy, 2> pool;
    Dummy* a = pool.Allocate();
    Dummy* b = pool.Allocate();
    pool.Deallocate(a);
    Dummy* c = pool.Allocate();
    EXPECT_EQ(a, c);  // 같은 슬롯 재사용
}

TEST(FixedObjectPoolTest, AllocatedObjectsAreDistinct) {
    Base::FixedObjectPool<Dummy, 3> pool;
    Dummy* a = pool.Allocate();
    Dummy* b = pool.Allocate();
    EXPECT_NE(a, b);   // 서로 다른 객체여야
}

TEST(FixedObjectPoolTest, AllocDeallocAlloc) {
    constexpr size_t cnt = 3000;
    Base::FixedObjectPool<Dummy, cnt> pool;
    Dummy* ptrs[cnt];
    for (int i = 0; i < cnt; i++)
    {
        ptrs[i] = pool.Allocate();
    }
    EXPECT_EQ(pool.Allocate(), nullptr);
    for (int i = 0; i < cnt; i++)
    {
        pool.Deallocate(ptrs[i]);
    }
    for (int i = 0; i < cnt; i++)
    {
        EXPECT_NE(pool.Allocate(), nullptr);
    }
    EXPECT_EQ(pool.Allocate(), nullptr);
}

TEST(FixedObjectPoolTest, MultiThreadSafe) {
    constexpr size_t SIZE = 50000;
    constexpr int THREADS = 5;
    constexpr int REQUEST = 10000;
    Base::FixedObjectPool<Dummy, SIZE> pool;

    std::vector<std::thread> threads;
    std::atomic<int> nullCount = 0;
    std::atomic<int> successCount = 0;

    std::set<Dummy*> allocated;
    std::mutex setMutex;

    for (int i = 0; i < THREADS; i++) 
    {
        threads.push_back(std::thread([&]() {
            std::set<Dummy*> local;
            for (int j = 0; j < REQUEST; j++) 
            {
                Dummy* d = pool.Allocate();
                if (d !=  nullptr) {
                    local.insert(d);
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
                else {
                    nullCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
            std::lock_guard<std::mutex> lock(setMutex);
            allocated.insert(local.begin(), local.end());
            }));
    }
    
    for (int i = 0; i < THREADS; i++) {
        threads[i].join();
    }
    EXPECT_EQ(allocated.size(), THREADS * REQUEST);

    for (Dummy* d: allocated)
    {
        pool.Deallocate(d);
    }

    EXPECT_EQ(nullCount, 0);
    EXPECT_EQ(successCount, THREADS * REQUEST);
}