#pragma once

#include <winsock2.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "Packet.h"
#include "PacketPool.h"
#include <CoreLib/IPacket.h>
namespace Net {
    enum class IOOperation : uint8_t
    {
        RECV, // 0
        SEND, // 1
        ACCEPT, // 2
    };

    struct alignas(64)  STOverlappedEx {
        WSAOVERLAPPED   wsaOverlapped;    //Overlapped IO 구조체
        SOCKET          clientSocket;     // 클라이언트 소켓
        std::vector<WSABUF> wsaBuf;           // 버퍼 정보를 담는 구조체,
        //버퍼 크기와 버퍼 포인터를 담고 있음, 버퍼 관리는 send는 PacketPool, recv는 ClientContext에서

        IOOperation op;
        std::unique_ptr<Core::IPacket, Core::PacketDeleter> uniquePacket; // 단일 대상 send 
        std::shared_ptr<Core::IPacket> sharedPacket; // 여러 대상 send
        std::vector<std::shared_ptr<Core::IPacket>> packetChunks;

        STOverlappedEx() {
            wsaBuf.reserve(9);
            packetChunks.reserve(9);
        }
    };

}
