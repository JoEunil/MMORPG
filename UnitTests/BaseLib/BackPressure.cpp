#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <queue>
#include <thread>
#include <vector>

#include <BaseLib/BackPressure.h>

namespace {
	class TestBucket {
		std::queue<int> m_queue;

	public:
		bool push(int& item) {
			if (m_queue.size() == 1) {
				return false;
			}
			m_queue.push(item);
			return true;
		}

		bool pop(int& out) {
			if (m_queue.empty()) {
				return false;
			}
			out = m_queue.front();
			m_queue.pop();
			return true;
		}
	};

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

	struct AlwaysFullBucket {
		bool push(BlockingItem&) {
			return false;
		}

		bool pop(BlockingItem&) {
			return false;
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

TEST(BackPressureTest, DegradedPathDropsDroppableAndDefersImportant) {
	using Bucket = Base::LockFreeQueue<int, 2>;
	Base::BackPressure<Bucket, int, 4> queue;

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

TEST(BackPressureTest, SupportsAnyBucketWithPushAndPop) {
	Base::BackPressure<TestBucket, int, 2> queue;

	int one = 1;
	int two = 2;
	EXPECT_TRUE(queue.Enqueue(one, Base::Priority::Droppable));
	EXPECT_TRUE(queue.Enqueue(two, Base::Priority::Important));

	int out = 0;
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 1);
	ASSERT_TRUE(queue.Dequeue(out));
	EXPECT_EQ(out, 2);
}

TEST(BackPressureTest, DeferredItemIsNotStrandedDuringRecoveryRace) {
	Base::BackPressure<AlwaysFullBucket, BlockingItem, 2> queue;
	std::atomic<bool> entered = false;
	std::atomic<bool> proceed = false;
	std::atomic<bool> accepted = false;
	BlockingItem::entered = &entered;
	BlockingItem::proceed = &proceed;

	BlockingItem item(7, true);
	std::thread producer([&]() {
		accepted.store(queue.Enqueue(item, Base::Priority::Important), std::memory_order_release);
	});

	const bool producerEntered = WaitUntilTrue(entered);
	EXPECT_TRUE(producerEntered);
	if (producerEntered) {
		BlockingItem out;
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
