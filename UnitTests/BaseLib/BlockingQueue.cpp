#include <gtest/gtest.h>

#include <memory>

#include <BaseLib/BlockingQueue.h>

struct TestItem {
	int item1;
	int item2;
};

TEST(BlockingQueueTest, BasicOperation) {
	Base::BlockingQueue<TestItem, 8, true> queue;
	TestItem item { 12, 34 };
	EXPECT_TRUE(queue.push(item));
	TestItem poppedItem;
	EXPECT_TRUE(queue.pop(poppedItem)); 
	EXPECT_EQ(poppedItem.item1, 12);
	EXPECT_EQ(poppedItem.item2, 34);
}

TEST(BlockingQueueTest, RvaluePush) {
	Base::BlockingQueue<TestItem, 8, true> queue;
	TestItem item{ 12, 34 };
	EXPECT_TRUE(queue.push(std::move(item)));
	TestItem poppedItem;
	EXPECT_TRUE(queue.pop(poppedItem));
	EXPECT_EQ(poppedItem.item1, item.item1);
	EXPECT_EQ(poppedItem.item2, item.item2);

	EXPECT_TRUE(queue.push(TestItem { 56, 78 }));
	EXPECT_TRUE(queue.pop(poppedItem));
	EXPECT_EQ(poppedItem.item1, 56);
	EXPECT_EQ(poppedItem.item2, 78);
}

TEST(BlockingQueueTest, unique_ptr) {
	Base::BlockingQueue<std::unique_ptr<TestItem>, 4, true> queue;
	std::unique_ptr<TestItem> item1(new TestItem(10, 13));
	std::unique_ptr<TestItem> item2(new TestItem(20, 23));
	std::unique_ptr<TestItem> item3(new TestItem(30, 33));
	std::unique_ptr<TestItem> item4(new TestItem(40, 43));
	std::unique_ptr<TestItem> item5(new TestItem(50, 53));

	EXPECT_TRUE(queue.push(std::move(item1)));
	EXPECT_TRUE(queue.push(std::move(item2)));
	EXPECT_TRUE(queue.push(std::move(item3)));
	EXPECT_TRUE(queue.push(std::move(item4)));
	EXPECT_FALSE(queue.push(std::move(item5)));
	EXPECT_EQ(item1, nullptr);
	EXPECT_EQ(item2, nullptr);
	EXPECT_EQ(item3, nullptr);
	EXPECT_EQ(item4, nullptr);
	EXPECT_EQ(item5->item1, 50);
	EXPECT_EQ(item5->item2, 53);

	std::unique_ptr<TestItem> poppedItem;
	for (int expected = 10; expected <= 40; expected += 10) {
		ASSERT_TRUE(queue.pop(poppedItem));
		EXPECT_EQ(poppedItem->item1, expected);
	}
	queue.Stop();
	EXPECT_FALSE(queue.pop(poppedItem));
}


TEST(BlockingQueueTest, Drain) {
	Base::BlockingQueue<TestItem, 8, true> queue;
	TestItem item{ 12, 34 };
	EXPECT_TRUE(queue.push(item));

	queue.Stop();

	EXPECT_TRUE(queue.push(item)); // push는 성공하지만, semaphore에서는 차단되기 때문에 permit이 증가하지 않는다.

	TestItem poppedItem;
	EXPECT_TRUE(queue.pop(poppedItem));
	EXPECT_EQ(poppedItem.item1, item.item1);
	EXPECT_EQ(poppedItem.item2, item.item2);
	EXPECT_FALSE(queue.pop(poppedItem));

}

TEST(BlockingQueueTest, DrainPendingItems) {
	constexpr int ItemCount = 1024;
	Base::BlockingQueue<TestItem, ItemCount, true> queue;

	for (int i = 0; i < ItemCount; ++i) {
		TestItem item{ i, i };
		ASSERT_TRUE(queue.push(item));
	}

	// phantom permit을 만들지 않는지 검증
	TestItem rejected{ ItemCount, ItemCount };
	EXPECT_FALSE(queue.push(rejected));

	queue.Stop();

	std::atomic<int> acquiredCount{ 0 };
	std::atomic<int64_t> acquiredSum{ 0 };
	std::vector<std::thread> threads;

	for (int i = 0; i < 4; ++i) {
		threads.emplace_back([&]() {
			TestItem item{};

			while (queue.pop(item)) {
				acquiredCount.fetch_add(1, std::memory_order_relaxed);
				acquiredSum.fetch_add(item.item1, std::memory_order_relaxed);
			}
			});
	}

	for (auto& thread : threads) {
		thread.join();
	}

	EXPECT_EQ(acquiredCount.load(), ItemCount);
	EXPECT_EQ(acquiredSum.load(), static_cast<int64_t>(ItemCount) * (ItemCount - 1) / 2);
}

TEST(BlockingQueueTest, HardStopDiscardsPendingItems) {
	constexpr int ItemCount = 1024;
	Base::BlockingQueue<TestItem, ItemCount, false> queue;

	for (int i = 0; i < ItemCount; ++i) {
		TestItem item{ i, i };
		ASSERT_TRUE(queue.push(item));
	}

	queue.Stop();

	std::atomic<int> acquiredCount{ 0 };
	std::vector<std::thread> threads;

	for (int i = 0; i < 4; ++i) {
		threads.emplace_back([&]() {
			TestItem item{};

			while (queue.pop(item)) {
				acquiredCount.fetch_add(1, std::memory_order_relaxed);
			}
			});
	}

	for (auto& thread : threads) {
		thread.join();
	}

	EXPECT_EQ(acquiredCount.load(), 0);
}

TEST(BlockingQueueTest, StopWakesAllWaiters) {
	Base::BlockingQueue<TestItem, 8, true> queue;

	std::atomic<int> entered{ 0 };
	std::atomic<int> stopped{ 0 };
	std::vector<std::thread> threads;

	for (int i = 0; i < 20; ++i) {
		threads.emplace_back([&]() {
			entered.fetch_add(1, std::memory_order_release);

			TestItem item{};
			if (!queue.pop(item)) {
				stopped.fetch_add(1, std::memory_order_relaxed);
			}
			});
	}

	while (entered.load(std::memory_order_acquire) != 20) {
		std::this_thread::yield();
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	queue.Stop();

	for (auto& thread : threads) {
		thread.join();
	}

	EXPECT_EQ(stopped.load(), 20);
}