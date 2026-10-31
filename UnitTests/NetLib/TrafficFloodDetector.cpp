#include <gtest/gtest.h>
#include <thread>

#include <NetLibrary/TrafficFloodDetector.h>
#include <NetLibrary/NetTimer.h>

class FloodDetectTest : public::testing::Test {
public:
	FloodDetectTest() {
		timer.StartThread();
	}
	Net::TrafficFloodDetector detector;
	Net::NetTimer timer;
};

TEST_F(FloodDetectTest, tinyPacketAttack) {
	for (int i = 0; i < Net::RECV_TOKEN_CAPACITY; i++) {
		detector.ByteReceived(10);
	}
	EXPECT_TRUE(detector.ByteReceived(10));
}

TEST_F(FloodDetectTest, tinyPacketSleep) {
	for (int i = 0; i < Net::RECV_TOKEN_CAPACITY; i++) {
		detector.ByteReceived(10);
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_FALSE(detector.ByteReceived(10));
}

TEST_F(FloodDetectTest, bigPacketAttack) {
	detector.ByteReceived(Net::BYTES_TOKEN_CAPACITY);
	EXPECT_TRUE(detector.ByteReceived(1));
}

TEST_F(FloodDetectTest, bigPacketSleep) {
	detector.ByteReceived(Net::BYTES_TOKEN_CAPACITY);
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_FALSE(detector.ByteReceived(1));
}
