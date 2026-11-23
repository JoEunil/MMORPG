#include <gtest/gtest.h>

#include <memory>
#include <CoreLib/World/ZoneThread.h>
#include <CoreLib/World/ZoneState.h>

TEST(ZoneEventQueueTest, FullQueueRejectsTransferButDefersProfileAndDisconnect) {
    auto queue = std::make_unique<Core::ZoneEventQueue>();
    for (uint32_t index = 0; index < Core::ZONE_QUEUE_SIZE; ++index) {
        Core::ZoneEvent event{Core::ZoneEventType::TransferRejected};
        ASSERT_TRUE(queue->Enqueue(event));
    }

    Core::ZoneEvent transfer{Core::ZoneEventType::Transfer};
    transfer.transfer = std::make_shared<Core::ZoneTransfer>();
    const auto* original = transfer.transfer.get();
    EXPECT_FALSE(queue->Enqueue(transfer));
    ASSERT_EQ(transfer.transfer.get(), original);

    Core::ZoneEvent profile{Core::ZoneEventType::ProfileVersion};
    Core::ZoneEvent disconnect{Core::ZoneEventType::Disconnect};
    disconnect.request.sessionID = 42;
    ASSERT_TRUE(queue->EnqueueDeferred(profile));
    ASSERT_TRUE(queue->EnqueueDeferred(disconnect));

    bool receivedProfile = false, receivedDisconnect = false;
    uint32_t received = 0;
    Core::ZoneEvent event;
    while (queue->Dequeue(event)) {
        ++received;
        if (event.type == Core::ZoneEventType::ProfileVersion) receivedProfile = true;
        if (event.type == Core::ZoneEventType::Disconnect) {
            receivedDisconnect = true;
            EXPECT_EQ(event.request.sessionID, 42);
        }
    }
    EXPECT_EQ(received, Core::ZONE_QUEUE_SIZE + 2);
    EXPECT_TRUE(receivedProfile);
    EXPECT_TRUE(receivedDisconnect);
    ASSERT_TRUE(queue->Enqueue(transfer));
    EXPECT_EQ(transfer.transfer, nullptr);
    ASSERT_TRUE(queue->Dequeue(event));
    EXPECT_EQ(event.transfer.get(), original);
}
