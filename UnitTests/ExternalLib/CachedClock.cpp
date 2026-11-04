#include <gtest/gtest.h>

#include <chrono>
#include <ExternalLib/CachedClock.h>

class CachedClockTest : public ::testing::Test {
protected:
	External::CachedClock timer;
	CachedClockTest() {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	void StopTimer() {
		timer.Stop();
	}
};

TEST_F(CachedClockTest, TimerDelay) {
	for (int i = 0; i < 20; i++)
	{
		auto now = std::chrono::steady_clock::now();
		uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// 갱신 주기가 Windows 타이머 해상도(약 15.6ms)에 종속되므로
		// 캐시 값은 그만큼 낡을 수 있다. 여유를 둬 2주기(약 31ms)를 상한으로 잡는다.
		EXPECT_TRUE(ms - timer.GetTimeMS() < 32);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
};

TEST_F(CachedClockTest, Stop) {
	StopTimer();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	uint64_t stopped = timer.GetTimeMS();
	for (int i = 0; i < 20; i++)
	{
		EXPECT_EQ(stopped, timer.GetTimeMS());
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}
