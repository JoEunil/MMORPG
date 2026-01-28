#pragma once

#include <memory>
namespace Core {
    class IPacket;
    class IPacketPool {
    public:
        virtual ~IPacketPool() = default;
        virtual std::shared_ptr<IPacket> Acquire() = 0;
        virtual std::unique_ptr<Core::IPacket> AcquireUnique() = 0;
        virtual void Return(IPacket* packet) = 0;
    };
}
