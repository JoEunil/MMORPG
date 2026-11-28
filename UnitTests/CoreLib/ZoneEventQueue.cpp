#include <gtest/gtest.h>

#include <memory>
#include <CoreLib/World/ZoneThread.h>
#include <CoreLib/World/ZoneState.h>

TEST(ZoneEventQueueTest, FullQueueRejectsTransferButDefersProfileAndDisconnect) {
    auto queue = std::make_unique<Core::ZoneEventQueue>();
    for (uint32_t index = 0; index < Core::ZONE_QUEUE_SIZE; ++index) {
        Core::ZoneEvent event = Core::ZoneTransferRejectedEvent{};
        ASSERT_EQ(queue->Enqueue(event), Base::EnqueueResult::Queued);
    }

    Core::ZoneEvent transfer = Core::ZoneTransferEvent{std::make_shared<Core::ZoneTransfer>()};
    const auto* original = std::get<Core::ZoneTransferEvent>(transfer).transfer.get();
    EXPECT_EQ(queue->Enqueue(transfer), Base::EnqueueResult::Rejected);
    ASSERT_EQ(std::get<Core::ZoneTransferEvent>(transfer).transfer.get(), original);

    Core::ZoneEvent profile = Core::ZoneProfileVersionEvent{};
    Core::ZoneEvent disconnect = Core::ZoneDisconnectEvent{42};
    ASSERT_EQ(queue->EnqueueDeferred(profile), Base::EnqueueResult::Queued);
    ASSERT_EQ(queue->EnqueueDeferred(disconnect), Base::EnqueueResult::Queued);

    bool receivedProfile = false, receivedDisconnect = false;
    uint32_t received = 0;
    Core::ZoneEvent event;
    while (queue->Dequeue(event)) {
        ++received;
        if (std::holds_alternative<Core::ZoneProfileVersionEvent>(event)) receivedProfile = true;
        if (const auto* disconnectEvent = std::get_if<Core::ZoneDisconnectEvent>(&event)) {
            receivedDisconnect = true;
            EXPECT_EQ(disconnectEvent->sessionID, 42);
        }
    }
    EXPECT_EQ(received, Core::ZONE_QUEUE_SIZE + 2);
    EXPECT_TRUE(receivedProfile);
    EXPECT_TRUE(receivedDisconnect);
    ASSERT_EQ(queue->Enqueue(transfer), Base::EnqueueResult::Queued);
    EXPECT_EQ(std::get<Core::ZoneTransferEvent>(transfer).transfer, nullptr);
    ASSERT_TRUE(queue->Dequeue(event));
    EXPECT_EQ(std::get<Core::ZoneTransferEvent>(event).transfer.get(), original);
}
