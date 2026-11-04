#include <gtest/gtest.h>

#include <thread>
#include <NetLibrary/TrafficFloodDetector.h>
#include <ExternalLib/CachedClock.h>

class FloodDetectTest : public::testing::Test {
public:
	Net::TrafficFloodDetector detector;
	External::CachedClock clock;
};

TEST_F(FloodDetectTest, tinyPacketAttack) {
	auto fixedTime = clock.GetTimeMS();
	for (int i = 0; i < Net::RECV_TOKEN_CAPACITY; i++) {
		detector.ByteReceived(10, fixedTime);
	}
	EXPECT_TRUE(detector.ByteReceived(10, fixedTime));
}

TEST_F(FloodDetectTest, tinyPacketSleep) {
	for (int i = 0; i < Net::RECV_TOKEN_CAPACITY; i++) {
		detector.ByteReceived(10, clock.GetTimeMS());
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_FALSE(detector.ByteReceived(10, clock.GetTimeMS()));
}

TEST_F(FloodDetectTest, bigPacketAttack) {
	auto fixedTime = clock.GetTimeMS();
	detector.ByteReceived(Net::BYTES_TOKEN_CAPACITY, fixedTime);
	EXPECT_TRUE(detector.ByteReceived(1, fixedTime));
}

TEST_F(FloodDetectTest, bigPacketSleep) {
	detector.ByteReceived(Net::BYTES_TOKEN_CAPACITY, clock.GetTimeMS());
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_FALSE(detector.ByteReceived(1, clock.GetTimeMS()));
}
