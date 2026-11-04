#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <utility>

#include <NetLibrary/PacketPoolManager.h>

TEST(PacketPoolManagerTest, ReturnToOriginPool) {
    auto manager = std::make_unique<Net::PacketPoolManager>();
    const std::array<std::pair<Core::PacketPoolType, uint32_t>, 3> cases{{
        {Core::PacketPoolType::Normal, Net::NORMAL_PACKET_LENGTH},
        {Core::PacketPoolType::Big, Net::BIG_PACKET_LENGTH},
        {Core::PacketPoolType::Giant, Net::GIANT_PACKET_LENGTH}
    }};
    std::array<uint32_t, 3> poolCapacity{};

    for (int i = 0; i < cases.size(); i++) {
        const auto [type, capacity] = cases[i];
        const auto before = manager->GetPoolSize(type);
		poolCapacity[i] = before;
        auto unique = manager->AcquireUnique(type);
        ASSERT_NE(unique, nullptr);
        EXPECT_EQ(unique->GetCapacity(), capacity);
        EXPECT_EQ(unique->GetLength(), 0);
        EXPECT_EQ(manager->GetPoolSize(type), before - 1);
    }

    for (int i = 0; i < cases.size(); i++) {
        const auto [type, capacity] = cases[i];
        EXPECT_EQ(manager->GetPoolSize(type), poolCapacity[i]);
    }
    for (int i = 0; i < cases.size(); i++) {
        const auto [type, capacity] = cases[i];
        auto unique = manager->AcquireUnique(type);
        ASSERT_NE(unique, nullptr);
        EXPECT_EQ(unique->GetCapacity(), capacity);
        EXPECT_EQ(unique->GetLength(), 0);
    }

}
