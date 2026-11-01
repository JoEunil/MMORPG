#include <gtest/gtest.h>
#include <NetLibrary/AccumulatedBuffer.h>

std::vector<uint8_t> testPtr(100000);

TEST(AccumulatedBufferTest, Clear) {
    Net::AccumulatedBuffer buf(&testPtr[0]);
    buf.Push(30);
    buf.Pop(10);
    buf.Clear();
    EXPECT_EQ(buf.GetFront(), 0);
    EXPECT_EQ(buf.GetRear(), Net::RING_BUFFER_SIZE - 1);
    EXPECT_EQ(buf.GetStartPtr(), &testPtr[0]);
}

TEST(AccumulatedBufferTest, Getter) {
    Net::AccumulatedBuffer buf(&testPtr[0]);
    buf.Push(30);
    EXPECT_EQ(buf.GetLen(), 30);
    buf.Pop(10);
    EXPECT_EQ(buf.GetFront(), 10);
    EXPECT_EQ(buf.GetRear(), 29);
    EXPECT_EQ(buf.GetLen(), 20);

    EXPECT_EQ(buf.GetStartPtr(), &testPtr[0]);
    EXPECT_EQ(buf.GetFrontPtr(), &testPtr[10]);
}

TEST(AccumulatedBufferTest, NextPos) {
    Net::AccumulatedBuffer buf(&testPtr[0]);
    buf.Push(30);
    buf.Pop(10);
    EXPECT_FALSE(buf.IsNextPos(&testPtr[29]));
    EXPECT_TRUE(buf.IsNextPos(&testPtr[30]));
}

TEST(AccumulatedBufferTest, WrapAround) {
    Net::AccumulatedBuffer buf(&testPtr[0]);
    buf.Push(Net::RING_BUFFER_SIZE / 2);
    buf.Push(Net::RING_BUFFER_SIZE / 2);
    EXPECT_EQ(Net::RING_BUFFER_SIZE, buf.GetLen());
    buf.Pop(Net::RING_BUFFER_SIZE / 2);
    buf.Push(Net::RING_BUFFER_SIZE / 2  - 20);
    EXPECT_EQ(Net::RING_BUFFER_SIZE - 20, buf.GetLen());
}