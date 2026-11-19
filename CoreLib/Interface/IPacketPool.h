#pragma once

#include <memory>
#include "IPacket.h"
namespace Core {
    class IPacketPool {
    public:
        virtual ~IPacketPool() = default;
        virtual std::shared_ptr<IPacket> Acquire() = 0;
        virtual std::unique_ptr<Core::IPacket, Core::PacketDeleter> AcquireUnique() = 0;
        virtual void Return(IPacket* packet) = 0;
    };
}
