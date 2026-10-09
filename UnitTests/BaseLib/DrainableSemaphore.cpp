#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <BaseLib/DrainableSemaphore.h>

TEST(DrainableSemaphoreTest, BasicOperation) {
	Base::DrainableSemaphore<true> drainableSemaphore;
	drainableSemaphore.Release();
	EXPECT_TRUE(drainableSemaphore.Acquire());
	drainableSemaphore.Release(2);
	EXPECT_TRUE(drainableSemaphore.Acquire());
	EXPECT_TRUE(drainableSemaphore.Acquire());
	drainableSemaphore.Release(3);
	EXPECT_TRUE(drainableSemaphore.Acquire());
	EXPECT_TRUE(drainableSemaphore.Acquire());
	EXPECT_TRUE(drainableSemaphore.Acquire());
}

TEST(DrainableSemaphoreTest, Drain) {
	Base::DrainableSemaphore<true> drainableSemaphore;
	drainableSemaphore.Release();
	drainableSemaphore.Close();
	EXPECT_TRUE(drainableSemaphore.Acquire());
	drainableSemaphore.Release();
	EXPECT_FALSE(drainableSemaphore.Acquire());
}

TEST(DrainableSemaphoreTest, HardStop) {
	Base::DrainableSemaphore<false> hardStopSemaphore;
	hardStopSemaphore.Release();
	hardStopSemaphore.Close();
	EXPECT_FALSE(hardStopSemaphore.Acquire());
}

TEST(DrainableSemaphoreTest, CloseIsIdempotent) {
	Base::DrainableSemaphore<true> semaphore;

	semaphore.Release(2);

	semaphore.Close();
	semaphore.Close();
	semaphore.Close();

	EXPECT_TRUE(semaphore.Acquire());
	EXPECT_TRUE(semaphore.Acquire());
	EXPECT_FALSE(semaphore.Acquire());

	semaphore.Close();
	EXPECT_FALSE(semaphore.Acquire());
}

TEST(DrainableSemaphoreTest, AcquireBlocksUntilRelease) {
	Base::DrainableSemaphore<true> semaphore;
	std::thread t([&]() {
		EXPECT_TRUE(semaphore.Acquire());
		EXPECT_TRUE(semaphore.Acquire());
		EXPECT_TRUE(semaphore.Acquire());
		EXPECT_TRUE(semaphore.Acquire());
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	semaphore.Release(4);
	t.join();
}

TEST(DrainableSemaphoreTest, DrainPendingPermits) {
	Base::DrainableSemaphore<true> semaphore;
	std::atomic<int> acquiredCount{ 0 };
	std::vector<std::thread> threads;

	semaphore.Release(100000);
	semaphore.Close();

	for (int i = 0; i < 4; i++) {
		threads.emplace_back([&]() {
			while (semaphore.Acquire()) {
				acquiredCount.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}


	for (int i = 0; i < 4; i++) {
		threads[i].join();
	}

	EXPECT_EQ(acquiredCount.load(std::memory_order_relaxed), 100000);
}

TEST(DrainableSemaphoreTest, HardStopDiscardsPendingPermits) {
	Base::DrainableSemaphore<false> semaphore;
	std::atomic<int> acquiredCount{ 0 };
	std::vector<std::thread> threads;

	semaphore.Release(100000);
	semaphore.Close();

	for (int i = 0; i < 4; i++) {
		threads.emplace_back([&]() {
			while (semaphore.Acquire()) {
				acquiredCount.fetch_add(1, std::memory_order_relaxed);
			}
			});
	}


	for (int i = 0; i < 4; i++) {
		threads[i].join();
	}

	EXPECT_EQ(acquiredCount.load(std::memory_order_relaxed), 0);
}

TEST(DrainableSemaphoreTest, CloseWakesAllWaiters) {
	Base::DrainableSemaphore<true> semaphore;
	std::atomic<int> entered{ 0 };
	std::atomic<int> stopped{ 0 };
	std::vector<std::thread> threads;

	for (int i = 0; i < 20; ++i) {
		threads.emplace_back([&]() {
			entered.fetch_add(1, std::memory_order_release);

			if (!semaphore.Acquire()) {
				stopped.fetch_add(1, std::memory_order_relaxed);
			}
			});
	}

	while (entered.load(std::memory_order_acquire) != 20) {
		std::this_thread::yield();
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	semaphore.Close();

	for (auto& thread : threads) {
		thread.join();
	}

	EXPECT_EQ(stopped.load(std::memory_order_relaxed), 20);
}