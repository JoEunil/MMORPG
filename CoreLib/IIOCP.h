#pragma once

#include <WinSock2.h>
#include <memory>
#include <cstdint>


namespace Core {
    class IPacket;
    class IIOCP {
    public:
        virtual ~IIOCP() = default;
        virtual void SendData(uint64_t sessionID, std::shared_ptr<IPacket> packet) = 0;
        virtual void SendDataUnique(uint64_t sessionID, std::unique_ptr<IPacket> packet) = 0;
    };
}
