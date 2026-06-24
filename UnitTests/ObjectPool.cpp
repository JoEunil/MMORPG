#include <gtest/gtest.h>

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