#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <utility>

#include <CoreLib/Cache/Message.h>
#include <CoreLib/Cache/MessagePool.h>
#include <CoreLib/Cache/MessageWriter.h>

TEST(MessageContractsTest, ScopedMessagesMoveAndReturnToTheirOriginalPool) {
    Core::MessagePool<1, 64> firstPool;
    Core::MessagePool<1, 64> secondPool;
    Core::IMessagePool& first = firstPool;
    Core::IMessagePool& second = secondPool;
    Core::IMessage* firstAddress = nullptr;
    Core::IMessage* secondAddress = nullptr;
    {
        auto source = first.AcquireScoped();
        auto destination = second.AcquireScoped();
        ASSERT_NE(source, nullptr);
        ASSERT_NE(destination, nullptr);
        firstAddress = source.get();
        secondAddress = destination.get();
        EXPECT_EQ(first.AcquireScoped(), nullptr);
        EXPECT_EQ(second.AcquireScoped(), nullptr);

        Core::ScopedMessage moved(std::move(source));
        EXPECT_EQ(source, nullptr);
        EXPECT_EQ(moved.get(), firstAddress);
        destination = std::move(moved);
        EXPECT_EQ(moved, nullptr);
        EXPECT_EQ(destination.get(), firstAddress);
        EXPECT_EQ(first.AcquireScoped(), nullptr);

        auto returnedSecond = second.AcquireScoped();
        ASSERT_NE(returnedSecond, nullptr);
        EXPECT_EQ(returnedSecond.get(), secondAddress);
        EXPECT_EQ(second.AcquireScoped(), nullptr);
    }

    auto returnedFirst = first.AcquireScoped();
    auto returnedSecond = second.AcquireScoped();
    ASSERT_NE(returnedFirst, nullptr);
    ASSERT_NE(returnedSecond, nullptr);
    EXPECT_EQ(returnedFirst.get(), firstAddress);
    EXPECT_EQ(returnedSecond.get(), secondAddress);
    EXPECT_EQ(first.AcquireScoped(), nullptr);
    EXPECT_EQ(second.AcquireScoped(), nullptr);
}

TEST(MessageContractsTest, ResponseWriterPreservesLayoutAndRejectsInsufficientCapacity) {
    Core::MessageWriter concreteWriter;
    const Core::ICacheResponseWriter& writer = concreteWriter;
    const Core::MsgHeader header{ 0x123456789abcdef0ULL, Core::MSG_INVENTORY_UPDATE_RES };
    Core::MsgInventoryUpdateResBody body{};
    body.resStatus = 1;
    body.characterID = 123;
    body.itemID = 456;
    body.itemQuantity = 7;
    body.slot = 8;
    using Response = Core::MsgStruct<Core::MsgInventoryUpdateResBody>;
    Core::Message response(sizeof(Response));
    ASSERT_TRUE(writer.WriteResponse(response, header, body));
    EXPECT_EQ(response.GetLength(), sizeof(Response));

    Core::MsgHeader decodedHeader{};
    Core::MsgInventoryUpdateResBody decodedBody{};
    std::memcpy(&decodedHeader, response.GetBuffer() + offsetof(Response, header), sizeof(decodedHeader));
    std::memcpy(&decodedBody, response.GetBuffer() + offsetof(Response, body), sizeof(decodedBody));
    EXPECT_EQ(decodedHeader.sessionID, header.sessionID);
    EXPECT_EQ(decodedHeader.messageType, header.messageType);
    EXPECT_EQ(decodedBody.resStatus, body.resStatus);
    EXPECT_EQ(decodedBody.characterID, body.characterID);
    EXPECT_EQ(decodedBody.itemID, body.itemID);
    EXPECT_EQ(decodedBody.itemQuantity, body.itemQuantity);
    EXPECT_EQ(decodedBody.slot, body.slot);

    // Exercise a body offset greater than sizeof(MsgHeader), plus trailing padding.
    struct alignas(32) AlignedBody { uint64_t value; };
    using AlignedResponse = Core::MsgStruct<AlignedBody>;
    static_assert(offsetof(AlignedResponse, body) > sizeof(Core::MsgHeader));
    Core::Message alignedResponse(sizeof(AlignedResponse));
    ASSERT_TRUE(concreteWriter.Write(alignedResponse, header, AlignedBody{ 42 }));
    EXPECT_EQ(alignedResponse.GetLength(), sizeof(AlignedResponse));
    uint64_t decodedValue = 0;
    std::memcpy(&decodedValue, alignedResponse.GetBuffer() + offsetof(AlignedResponse, body), sizeof(decodedValue));
    EXPECT_EQ(decodedValue, 42);

    constexpr auto shortCapacity = sizeof(Response) - 1;
    Core::Message tooSmall(shortCapacity);
    std::array<uint8_t, shortCapacity> original;
    original.fill(0xa5);
    std::memcpy(tooSmall.GetBuffer(), original.data(), original.size());
    tooSmall.SetLength(3);
    EXPECT_FALSE(writer.WriteResponse(tooSmall, header, body));
    EXPECT_EQ(tooSmall.GetLength(), 3);
    EXPECT_EQ(std::memcmp(tooSmall.GetBuffer(), original.data(), original.size()), 0);
}
