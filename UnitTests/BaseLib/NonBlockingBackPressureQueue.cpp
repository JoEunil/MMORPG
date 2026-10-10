#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <BaseLib/NonBlockingBackPressureQueue.h>

namespace {
	struct BlockingItem {
		static inline std::atomic<bool>* entered = nullptr;
		static inline std::atomic<bool>* proceed = nullptr;

		int value = 0;
		bool blockOnMove = false;

		BlockingItem() = default;
		BlockingItem(int value, bool blockOnMove)
			: value(value), blockOnMove(blockOnMove) {
		}

		BlockingItem& operator=(BlockingItem&& other) noexcept {
			if (other.blockOnMove && entered && proceed) {
				entered->store(true, std::memory_order_release);
				while (!proceed->load(std::memory_order_acquire)) {
					std::this_thread::yield();
				}
			}

			value = other.value;
			blockOnMove = false;
			other.value = 0;
			other.blockOnMove = false;
			return *this;
		}
	};

	bool WaitUntilTrue(const std::atomic<bool>& value) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!value.load(std::memory_order_acquire)) {
			if (std::chrono::steady_clock::now() >= deadline) {
				return false;
			}
			std::this_thread::yield();
		}
		return true;
	}
}

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

TEST(NonBlockingBackPressureQueueTest, PrimaryCapacityIsTemplateParameter) {
	Base::NonBlockingBackPressureQueue<int, 4, 2> queue;

	int one = 1;
	int two = 2;
	int three = 3;
	int four = 4;
	int droppable = 5;
	EXPECT_TRUE(queue.Enqueue(one, Base::Priority::Droppable));
	EXPECT_TRUE(queue.Enqueue(two, Base::Priority::Droppable));
	EXPECT_TRUE(queue.Enqueue(three, Base::Priority::Droppable));
	EXPECT_TRUE(queue.Enqueue(four, Base::Priority::Droppable));
	EXPECT_FALSE(queue.Enqueue(droppable, Base::Priority::Droppable));

	int out = 0;
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 1);
}

TEST(NonBlockingBackPressureQueueTest, DeferredItemIsNotStrandedDuringRecoveryRace) {
	Base::NonBlockingBackPressureQueue<BlockingItem, 2, 2> queue;
	std::atomic<bool> entered = false;
	std::atomic<bool> proceed = false;
	std::atomic<bool> accepted = false;
	BlockingItem::entered = &entered;
	BlockingItem::proceed = &proceed;
	BlockingItem first(1, false);
	BlockingItem second(2, false);
	ASSERT_TRUE(queue.Enqueue(first, Base::Priority::Droppable));
	ASSERT_TRUE(queue.Enqueue(second, Base::Priority::Droppable));

	BlockingItem item(7, true);
	std::thread producer([&]() {
		accepted.store(queue.Enqueue(item, Base::Priority::Important), std::memory_order_release);
	});

	const bool producerEntered = WaitUntilTrue(entered);
	EXPECT_TRUE(producerEntered);
	if (producerEntered) {
		BlockingItem out;
		EXPECT_TRUE(queue.Dequeue(out));
		EXPECT_EQ(out.value, 1);
		EXPECT_TRUE(queue.Dequeue(out));
		EXPECT_EQ(out.value, 2);
		EXPECT_FALSE(queue.Dequeue(out));
	}

	proceed.store(true, std::memory_order_release);
	producer.join();
	BlockingItem::entered = nullptr;
	BlockingItem::proceed = nullptr;

	EXPECT_TRUE(accepted.load(std::memory_order_acquire));
	BlockingItem out;
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out.value, 7);
}
