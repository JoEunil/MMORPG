#include <gtest/gtest.h>

#include <BaseLib/BlockingBackPressureQueue.h>

TEST(BlockingBackPressureQueueTest, DropsDroppableAndDefersImportant) {
	Base::BlockingBackPressureQueue<int, 2, 4, true> queue;

	int one = 1;
	int two = 2;
	int important = 3;
	int droppable = 4;
	int nextImportant = 5;

	EXPECT_TRUE(queue.Enqueue(one, Base::Priority::Droppable));
	EXPECT_TRUE(queue.Enqueue(two, Base::Priority::Droppable));
	EXPECT_TRUE(queue.Enqueue(important, Base::Priority::Important));
	EXPECT_FALSE(queue.Enqueue(droppable, Base::Priority::Droppable));
	EXPECT_TRUE(queue.Enqueue(nextImportant, Base::Priority::Important));

	int out = 0;
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 3);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 5);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 1);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 2);

	EXPECT_TRUE(queue.Enqueue(droppable, Base::Priority::Droppable));
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 4);
}


TEST(BlockingBackPressureQueueTest, DurableLog) {
	int logged = 0;
	Base::BlockingBackPressureQueue<int, 2, 2, true> queue(
		[&](const int& item) {
			logged = item;
			return true;
		});

	int one = 1;
	int two = 2;
	int three = 3;
	int four = 4;
	int five = 5;
	EXPECT_TRUE(queue.Enqueue(one, Base::Priority::Important));
	EXPECT_TRUE(queue.Enqueue(two, Base::Priority::Important));
	EXPECT_TRUE(queue.Enqueue(three, Base::Priority::Important));
	EXPECT_TRUE(queue.Enqueue(four, Base::Priority::Important));
	EXPECT_TRUE(queue.Enqueue(five, Base::Priority::Important));
	EXPECT_EQ(logged, 5);
}

TEST(BlockingBackPressureQueueTest, Drain) {
	Base::BlockingBackPressureQueue<int, 2, 2, true> queue;
	int one = 1;
	int two = 2;
	EXPECT_TRUE(queue.Enqueue(one, Base::Priority::Important));
	EXPECT_TRUE(queue.Enqueue(two, Base::Priority::Important));

	queue.Stop();

	int out = 0;
	EXPECT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 1);
	EXPECT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 2);
	EXPECT_FALSE(queue.Dequeue(out));
}

TEST(BlockingBackPressureQueueTest, HardStop) {
	Base::BlockingBackPressureQueue<int, 2, 2, false> queue;
	int item = 1;
	EXPECT_TRUE(queue.Enqueue(item, Base::Priority::Important));

	queue.Stop();

	int out = 0;
	EXPECT_FALSE(queue.Dequeue(out));
}
