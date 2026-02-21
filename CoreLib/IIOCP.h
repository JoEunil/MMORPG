#pragma once

#include <WinSock2.h>
#include <memory>
#include <cstdint>

#include <CoreLib/IPacket.h>

namespace Core {
    class IIOCP {
    public:
        virtual ~IIOCP() = default;
        virtual void SendData(uint64_t sessionID, std::shared_ptr<IPacket> packet) = 0;
        virtual void SendDataChunks(uint64_t sessionID, std::shared_ptr<IPacket> packet, std::vector<std::shared_ptr<IPacket>>& chunks) = 0;
        virtual void SendDataUnique(uint64_t sessionID, std::unique_ptr<IPacket, PacketDeleter> packet) = 0;
    };
}
