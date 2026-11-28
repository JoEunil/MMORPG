#include <gtest/gtest.h>

#include <vector>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <BaseLib/NonBlockingBackPressureQueue.h>

TEST(NonBlockingBackPressureQueueTest, DegradedPathDropsDroppableAndDefersImportant) {
	Base::NonBlockingBackPressureQueue<int, 2, 4> queue;

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

	std::vector<int> drained;
	int out = 0;
	while (queue.Dequeue(out)) {
		drained.push_back(out);
	}

	EXPECT_EQ(drained, (std::vector<int>{ 1, 2, 3, 5 }));

	int recovered = 6;
	EXPECT_EQ(queue.Enqueue(recovered, Base::Priority::Droppable), Base::EnqueueResult::Queued);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 6);
}

TEST(NonBlockingBackPressureQueueTest, EmptyQueueReturnsFalse) {
	Base::NonBlockingBackPressureQueue<int, 2, 2> queue;
	int out = 0;
	EXPECT_FALSE(queue.Dequeue(out));
}

TEST(NonBlockingBackPressureQueueTest, DynamicDeferPreservesMoveOnlyWorkAndRecovers) {
	Base::NonBlockingBackPressureQueue<std::unique_ptr<int>, 2, 0> queue;
	for (int value = 0; value < 34; ++value) {
		auto item = std::make_unique<int>(value);
		ASSERT_EQ(queue.EnqueueDeferred(item), Base::EnqueueResult::Queued);
		EXPECT_EQ(item, nullptr);
	}
	auto rejected = std::make_unique<int>(99);
	EXPECT_EQ(queue.Enqueue(rejected), Base::EnqueueResult::Rejected);
	ASSERT_NE(rejected, nullptr);
	EXPECT_EQ(*rejected, 99);

	std::vector<int> received;
	std::unique_ptr<int> item;
	while (queue.Dequeue(item)) received.push_back(*item);
	std::sort(received.begin(), received.end());
	ASSERT_EQ(received.size(), 34);
	for (int value = 0; value < 34; ++value) EXPECT_EQ(received[value], value);
	ASSERT_EQ(queue.Enqueue(rejected), Base::EnqueueResult::Queued);
	ASSERT_TRUE(queue.Dequeue(item));
	EXPECT_EQ(*item, 99);
}

TEST(NonBlockingBackPressureQueueTest, DynamicDeferSupportsConcurrentProducersAndConsumers) {
	constexpr int perProducer = 2000;
	constexpr int total = perProducer * 2 + 2;
	Base::NonBlockingBackPressureQueue<std::unique_ptr<int>, 2, 0> queue;
	std::array<std::atomic<int>, total> seen{};
	std::atomic<int> consumed = 0;
	std::atomic<bool> failed = false;
	for (int value = 0; value < 2; ++value) ASSERT_EQ(queue.EnqueueDeferred(std::make_unique<int>(value)), Base::EnqueueResult::Queued);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	auto consume = [&] {
		std::unique_ptr<int> item;
		while (consumed.load() < total && std::chrono::steady_clock::now() < deadline) {
			if (!queue.Dequeue(item)) { std::this_thread::yield(); continue; }
			if (*item < 0 || *item >= total) failed.store(true);
			else seen[*item].fetch_add(1);
			consumed.fetch_add(1);
		}
	};
	std::thread firstConsumer(consume), secondConsumer(consume);
	auto produce = [&](int producer) {
		for (int offset = 0; offset < perProducer; ++offset)
			if (queue.EnqueueDeferred(std::make_unique<int>(2 + producer * perProducer + offset)) == Base::EnqueueResult::Rejected) failed.store(true);
	};
	std::thread firstProducer(produce, 0), secondProducer(produce, 1);
	firstProducer.join(); secondProducer.join(); firstConsumer.join(); secondConsumer.join();
	EXPECT_FALSE(failed.load());
	ASSERT_EQ(consumed.load(), total);
	for (const auto& count : seen) EXPECT_EQ(count.load(), 1);
}

TEST(NonBlockingBackPressureQueueTest, DurableLog) {
	int logged = 0;
	Base::NonBlockingBackPressureQueue<int, 2, 2> queue;
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
	std::vector<int> received;
	int out = 0;
	while (queue.Dequeue(out)) received.push_back(out);
	std::sort(received.begin(), received.end());
	EXPECT_EQ(received, (std::vector<int>{1, 2, 3, 4}));
}
