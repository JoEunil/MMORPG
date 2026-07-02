#include <gtest/gtest.h>

#include <BaseLib/RingQueue.h>

TEST(RingQueueTest, InitStatus) {
	Base::RingQueue<int, 4> q;
	EXPECT_EQ(q.size(), 0);
	EXPECT_TRUE(q.empty());
	EXPECT_FALSE(q.full());
}

TEST(RingQueueTest, BasicOperations) {
	Base::RingQueue<int, 4> q;
	q.push(1);
	EXPECT_EQ(q.size(), 1);
	EXPECT_FALSE(q.empty());
	EXPECT_FALSE(q.full());
	q.push(2);
	q.push(3);
	EXPECT_EQ(q.size(), 3);
	EXPECT_FALSE(q.empty());
	EXPECT_TRUE(q.full());

	EXPECT_EQ(q.pop(), 1);
	EXPECT_EQ(q.pop(), 2);
	EXPECT_EQ(q.pop(), 3);

	EXPECT_TRUE(q.empty());
	EXPECT_FALSE(q.full());
}