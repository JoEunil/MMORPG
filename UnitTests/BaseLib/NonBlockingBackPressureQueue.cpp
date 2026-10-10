#include <gtest/gtest.h>

#include <vector>

#include <BaseLib/NonBlockingBackPressureQueue.h>

TEST(NonBlockingBackPressureQueueTest, DegradedPathDropsDroppableAndDefersImportant) {
	Base::NonBlockingBackPressureQueue<int, 2, 4> queue;

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

	std::vector<int> drained;
	int out = 0;
	while (queue.Dequeue(out)) {
		drained.push_back(out);
	}

	EXPECT_EQ(drained, (std::vector<int>{ 1, 2, 3, 5 }));

	int recovered = 6;
	EXPECT_TRUE(queue.Enqueue(recovered, Base::Priority::Droppable));
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 6);
}

TEST(NonBlockingBackPressureQueueTest, EmptyQueueReturnsFalse) {
	Base::NonBlockingBackPressureQueue<int, 2, 2> queue;
	int out = 0;
	EXPECT_FALSE(queue.Dequeue(out));
}

TEST(NonBlockingBackPressureQueueTest, DurableLog) {
	int logged = 0;
	Base::NonBlockingBackPressureQueue<int, 2, 2> queue(
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
