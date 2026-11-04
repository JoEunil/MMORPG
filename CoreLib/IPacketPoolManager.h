#pragma once

#include <cstdint>
#include <memory>
#include "IPacket.h"

namespace Core {
    enum class PacketPoolType : uint8_t {
        Normal,
        Big,
        Giant,
    };

    class IPacketPoolManager {
    public:
        virtual ~IPacketPoolManager() = default;
        virtual std::shared_ptr<IPacket> Acquire(PacketPoolType type) = 0;
        virtual std::unique_ptr<IPacket, PacketDeleter> AcquireUnique(PacketPoolType type) = 0;
    };
}
