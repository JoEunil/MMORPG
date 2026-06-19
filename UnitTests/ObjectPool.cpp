#include <gtest/gtest.h>

#include <BaseLib/ObjectPool.h>
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

TEST(FixedObjectPoolTest, AllocatedObjectsAreDistinct) {
    Base::FixedObjectPool<Dummy, 3> pool;
    Dummy* a = pool.Allocate();
    Dummy* b = pool.Allocate();
    EXPECT_NE(a, b);   // 서로 다른 객체여야
}

TEST(ObjectPoolTest, InitialTargetSize) {
    Base::ObjectPool<Dummy> pool(4, 8, 2);  // target=4, max=8, min=2
    // target개로 시작했는지 — Acquire로 간접 확인
    for (int i = 0; i < 4; ++i) {
        Dummy* p = pool.Acquire();
        ASSERT_NE(p, nullptr) << "i=" << i;
        pool.Return(p);
    }
}

TEST(ObjectPoolTest, AcquireReturnsValid) {
    Base::ObjectPool<Dummy> pool(4, 8, 2);
    Dummy* p = pool.Acquire();
    ASSERT_NE(p, nullptr);
    pool.Return(p);
}

TEST(ObjectPoolTest, ExpandsBelowMin) {
    Base::ObjectPool<Dummy> pool(4, 8, 2);
    // min(2) 이하로 떨어지면 target까지 늘어나야 — 계속 Acquire 해도 nullptr 안 나와야
    std::vector<Dummy*> held;
    for (int i = 0; i < 6; ++i) {
        Dummy* p = pool.Acquire();
        EXPECT_NE(p, nullptr) << "i=" << i;  // elastic이라 계속 나와야
        if (p) held.push_back(p);
    }
    for (auto* p : held) pool.Return(p);
}