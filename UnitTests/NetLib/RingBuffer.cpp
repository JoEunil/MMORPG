#include <gtest/gtest.h>

#include <NetLibrary/RingBuffer.h>

class RingBufferTest : public ::testing::Test {
protected:
	Net::RingBuffer buf;
	void Initialize(uint32_t size) {
		buf.Initialize(size); 
	}
};

TEST_F(RingBufferTest, InitStatus) {
	Initialize(4);
	ASSERT_NE(buf.GetStartPtr(), nullptr);
	EXPECT_EQ(4, buf.GetCapacity()) ;	
}

TEST_F(RingBufferTest, AcquireBuffer) {
	Net::BufferFragment fragment;
	Initialize(4);
	uint16_t len;
	len = buf.TryAcquireBuffer(fragment, 2);
	EXPECT_EQ(buf.GetStartPtr() + fragment.front, fragment.startPtr);
	EXPECT_EQ(fragment.rear - fragment.front + 1, fragment.length);
	EXPECT_EQ(2, fragment.length);
	EXPECT_EQ(2, len);

	len = buf.TryAcquireBuffer(fragment, 1);
	EXPECT_EQ(buf.GetStartPtr() + fragment.front, fragment.startPtr);
	EXPECT_EQ(fragment.rear - fragment.front + 1, fragment.length);
	EXPECT_EQ(1, fragment.length);
	EXPECT_EQ(1, len);

}

TEST_F(RingBufferTest, AcquireBufferZeroLen) {
	Net::BufferFragment fragment;
	Initialize(4);
	buf.TryAcquireBuffer(fragment, 0);
	EXPECT_EQ(buf.GetStartPtr() + fragment.front, fragment.startPtr);
	EXPECT_EQ(0, fragment.length);
}

TEST_F(RingBufferTest, AcquireBufferExhausted) {
	Net::BufferFragment fragment;
	Initialize(4);

	buf.TryAcquireBuffer(fragment, 4);
	EXPECT_EQ(4, fragment.length);

	buf.TryAcquireBuffer(fragment, 2);
	EXPECT_EQ(buf.GetStartPtr() + fragment.front, fragment.startPtr);
	EXPECT_EQ(0, fragment.length);
}

TEST_F(RingBufferTest, AcquireBufferRemaining) {
	Net::BufferFragment fragment;
	Initialize(4);
	
	buf.TryAcquireBuffer(fragment, 3);
	EXPECT_EQ(3, fragment.length);

	buf.TryAcquireBuffer(fragment, 4);
	EXPECT_EQ(buf.GetStartPtr() + fragment.front, fragment.startPtr);
	EXPECT_EQ(1, fragment.length);
}

TEST_F(RingBufferTest, AcquireAtLastPtr) {
	Net::BufferFragment fragment;
	Initialize(4);
	buf.TryAcquireBuffer(fragment, 4);
	EXPECT_EQ(4, fragment.length);
	buf.Release(fragment.front, fragment.rear);

	buf.TryAcquireBuffer(fragment, 4);
	EXPECT_EQ(4, fragment.length);
}

TEST_F(RingBufferTest, ReleaseBuffer) {
	Net::BufferFragment fragment;
	Initialize(4);
	
	buf.TryAcquireBuffer(fragment, 2);
	EXPECT_EQ(2, fragment.length);
	bool success;

	success = buf.Release(fragment.front, fragment.rear-1);
	EXPECT_TRUE(success);

	buf.TryAcquireBuffer(fragment, 4);
	EXPECT_EQ(3, fragment.length);

	success = buf.Release(fragment.front, fragment.rear-1);
	EXPECT_TRUE(success);

	buf.TryAcquireBuffer(fragment, 4);
	EXPECT_EQ(3, fragment.length);
}

TEST_F(RingBufferTest, ReleaseRejectsInvalidRange) {
	Net::BufferFragment fragment;
	Initialize(4);

	buf.TryAcquireBuffer(fragment, 2);
	EXPECT_EQ(2, fragment.length);
	bool success;

	success = buf.Release(fragment.front - 1, fragment.rear);
	EXPECT_FALSE(success);
	success = buf.Release(fragment.front, fragment.rear + 1);
	EXPECT_FALSE(success);
}

TEST_F(RingBufferTest, ReleaseLeftOver) {
	Net::BufferFragment fragment;
	Initialize(4);

	buf.TryAcquireBuffer(fragment, 3);
	EXPECT_EQ(3, fragment.length);

	buf.ReleaseLeftOver(fragment.front+3, true); // 2바이트 사용, 1바이트 반납

	buf.TryAcquireBuffer(fragment, 4);
	EXPECT_EQ(2, fragment.length);

}