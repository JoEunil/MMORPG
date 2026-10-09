#include <gtest/gtest.h>

#include <memory>

#include <BaseLib/RingQueue.h>

TEST(RingQueueTest, InitStatus) {
	Base::RingQueue<int, 4> q;
	EXPECT_EQ(q.size(), 0);
	EXPECT_TRUE(q.empty());
	EXPECT_FALSE(q.full());
}

TEST(RingQueueTest, BasicOperations) {
	Base::RingQueue<int, 4> q;
	EXPECT_TRUE(q.push(1));
	EXPECT_EQ(q.size(), 1);
	EXPECT_FALSE(q.empty());
	EXPECT_FALSE(q.full());
	EXPECT_TRUE(q.push(2));
	EXPECT_TRUE(q.push(3));
	EXPECT_EQ(q.size(), 3);
	EXPECT_FALSE(q.empty());
	EXPECT_TRUE(q.full());

	int out = 0;
	ASSERT_TRUE(q.pop(out));
	EXPECT_EQ(out, 1);
	ASSERT_TRUE(q.pop(out));
	EXPECT_EQ(out, 2);
	ASSERT_TRUE(q.pop(out));
	EXPECT_EQ(out, 3);

	EXPECT_TRUE(q.empty());
	EXPECT_FALSE(q.full());
}

TEST(RingQueueTest, FailedRvaluePush) {
	Base::RingQueue<std::unique_ptr<int>, 4> q;
	ASSERT_TRUE(q.push(std::make_unique<int>(1)));
	ASSERT_TRUE(q.push(std::make_unique<int>(2)));
	ASSERT_TRUE(q.push(std::make_unique<int>(3)));

	auto rejected = std::make_unique<int>(4);
	EXPECT_FALSE(q.push(std::move(rejected)));
	ASSERT_NE(rejected, nullptr);
	EXPECT_EQ(*rejected, 4);

	std::unique_ptr<int> out;
	ASSERT_TRUE(q.pop(out));
	EXPECT_EQ(*out, 1);
}
