#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include "../Data/PotionType.h"
#include "../Packet/PacketTypes.h"

namespace Core {
    struct ItemUseState {
        ItemUseResponseBody lastResponse = {};
        std::array<uint32_t, static_cast<std::size_t>(Data::PotionType::Count)> cooldownTicks{};

        void AdvanceTick() noexcept {
            for (auto& remaining : cooldownTicks)
                if (remaining != 0)
                    --remaining;
        }
    };
}
