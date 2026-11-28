#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

#include <BaseLib/BlockingBackPressureQueue.h>

TEST(BlockingBackPressureQueueTest, DropsDroppableAndDefersImportant) {
	Base::BlockingBackPressureQueue<int, 2, 4, true> queue;

	int one = 1;
	int two = 2;
	int important = 3;
	int droppable = 4;
	int nextImportant = 5;

	EXPECT_EQ(queue.Enqueue(one, Base::Priority::Droppable), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(two, Base::Priority::Droppable), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(important, Base::Priority::Important), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(droppable, Base::Priority::Droppable), Base::EnqueueResult::Rejected);
	EXPECT_EQ(queue.Enqueue(nextImportant, Base::Priority::Important), Base::EnqueueResult::Queued);

	int out = 0;
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 3);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 5);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 1);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 2);

	EXPECT_EQ(queue.Enqueue(droppable, Base::Priority::Droppable), Base::EnqueueResult::Queued);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 4);
}


TEST(BlockingBackPressureQueueTest, DurableLog) {
	int logged = 0;
	Base::BlockingBackPressureQueue<int, 2, 2, true> queue;
	queue.SetDurableLog([&](const int& item) {
			logged = item;
			return true;
	});

	int one = 1;
	int two = 2;
	int three = 3;
	int four = 4;
	int five = 5;
	EXPECT_EQ(queue.Enqueue(one, Base::Priority::Important), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(two, Base::Priority::Important), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(three, Base::Priority::Important), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(four, Base::Priority::Important), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(five, Base::Priority::Important), Base::EnqueueResult::RecordedForRecovery);
	EXPECT_EQ(logged, 5);
	queue.SetDurableLog([](const int&) { return false; });
	EXPECT_EQ(queue.Enqueue(6, Base::Priority::Important), Base::EnqueueResult::Rejected);
	queue.SetDurableLog({});
	EXPECT_EQ(queue.Enqueue(7, Base::Priority::Important), Base::EnqueueResult::Rejected);
	queue.Stop();
	std::vector<int> received;
	int out = 0;
	while (queue.Dequeue(out)) received.push_back(out);
	std::sort(received.begin(), received.end());
	EXPECT_EQ(received, (std::vector<int>{1, 2, 3, 4}));
}

TEST(BlockingBackPressureQueueTest, Drain) {
	Base::BlockingBackPressureQueue<int, 2, 2, true> queue;
	int one = 1;
	int two = 2;
	EXPECT_EQ(queue.Enqueue(one, Base::Priority::Important), Base::EnqueueResult::Queued);
	EXPECT_EQ(queue.Enqueue(two, Base::Priority::Important), Base::EnqueueResult::Queued);

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
	EXPECT_EQ(queue.Enqueue(item, Base::Priority::Important), Base::EnqueueResult::Queued);

	queue.Stop();

	int out = 0;
	EXPECT_FALSE(queue.Dequeue(out));
}
