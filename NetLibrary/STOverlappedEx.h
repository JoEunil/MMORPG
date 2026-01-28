#pragma once

#include <winsock2.h>
#include <cstdint>
#include <memory>
#include "Packet.h"
#include "PacketPool.h"
namespace Net {
    enum class IOOperation : uint8_t
    {
        RECV, // 0
        SEND, // 1
        ACCEPT, // 2
    };

    struct STOverlappedEx {
        WSAOVERLAPPED   wsaOverlapped;    //Overlapped IO 구조체
        SOCKET          clientSocket;     // 클라이언트 소켓
        WSABUF          wsaBuf;           // 버퍼 정보를 담는 구조체,
        //버퍼 크기와 버퍼 포인터를 담고 있음, 버퍼 관리는 send는 PacketPool, recv는 ClientContext에서
        IOOperation op;
        std::unique_ptr<Packet> uniquePacket; // 단일 대상 send 
        std::shared_ptr<Packet> sharedPacket; // 여러 대상 send
    };
}
