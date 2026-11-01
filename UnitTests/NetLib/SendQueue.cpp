#include <gtest/gtest.h>

#include <NetLibrary/SendQueue.h>

TEST(SendQueueTest, SendQueueFull) {
	Net::SendQueue sendQueue;
	EXPECT_EQ(sendQueue.Enqueue(new Net::STOverlappedEx()), Net::EnqueueSendResult::Ready);
	for (int i = 0; i < Net::SEND_QUEUE_SIZE-1; i++) {
		EXPECT_EQ(sendQueue.Enqueue(new Net::STOverlappedEx()), Net::EnqueueSendResult::Queued);
	}
	EXPECT_EQ(sendQueue.Enqueue(new Net::STOverlappedEx()), Net::EnqueueSendResult::QueueFull);
}

TEST(SendQueueTest, SendQueueEmpty) {
	Net::SendQueue sendQueue;
	EXPECT_EQ(sendQueue.Dequeue(), nullptr);
}

TEST(SendQueueTest, InFlight) {
	Net::SendQueue sendQueue;
	EXPECT_EQ(sendQueue.Enqueue(new Net::STOverlappedEx()), Net::EnqueueSendResult::Ready);
	EXPECT_EQ(sendQueue.Enqueue(new Net::STOverlappedEx()), Net::EnqueueSendResult::Queued);
	EXPECT_NE(sendQueue.Dequeue(), nullptr);
	EXPECT_EQ(sendQueue.Enqueue(new Net::STOverlappedEx()), Net::EnqueueSendResult::Queued);
	EXPECT_NE(sendQueue.Dequeue(), nullptr);
	EXPECT_EQ(sendQueue.Dequeue(), nullptr);
	EXPECT_EQ(sendQueue.Enqueue(new Net::STOverlappedEx()), Net::EnqueueSendResult::Ready);
}