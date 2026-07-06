#include <gtest/gtest.h>

#include <chrono>
#include <NetLibrary/NetTimer.h>

class NetTimerTest : public ::testing::Test {
protected:
	Net::NetTimer timer;
	NetTimerTest() {
		timer.StartThread();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	~NetTimerTest() {
		timer.Stop();
	}
	void StopTimer() {
		timer.Stop();
	}
};

TEST_F(NetTimerTest, TimerDelay) {
	for (int i = 0; i < 20; i++)
	{
		auto now = std::chrono::steady_clock::now();
		uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// 이론적으로 최대 오차는 17ms (windows timer 해상도 16.6sm)
		EXPECT_TRUE(ms - timer.GetTimeMS() < 32);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
};

TEST_F(NetTimerTest, Stop) {
	StopTimer();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	uint64_t stopped = timer.GetTimeMS();
	for (int i = 0; i < 20; i++)
	{
		EXPECT_EQ(stopped, timer.GetTimeMS());
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}
