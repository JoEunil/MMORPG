#pragma once

#include <winsock2.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "Packet.h"
#include "PacketPool.h"

#include <CoreLib/IPacket.h>
#include <CoreLib/Config.h>

namespace Net {
    class ClientContext;

    enum class IOOperation : uint8_t
    {
        RECV, // 0
        SEND, // 1
        ACCEPT, // 2
    };

    struct alignas(64)  STOverlappedEx {
        WSAOVERLAPPED   wsaOverlapped{};    //Overlapped IO 구조체
        int totalBytes = 0;
        int sentBytes = 0;
        SOCKET          clientSocket = INVALID_SOCKET;     // 클라이언트 소켓
        ClientContext*  ownerContext = nullptr; // RECV 버퍼를 소유한 Context. completion까지 재사용 금지
        uint64_t        sessionID = 0;     // 등록 당시 세션. 소켓/Context 재사용 검증용
        std::vector<WSABUF> wsaBuf;           // 버퍼 정보를 담는 구조체,
        std::vector<WSABUF> originalBufs;    // 최초의 Send 시점의 Buffer, Partial Send 처리에 사용. 
        //버퍼 크기와 버퍼 포인터를 담고 있음, 버퍼 관리는 send는 PacketPool, recv는 ClientContext에서

        IOOperation op = IOOperation::RECV;
        std::unique_ptr<Core::IPacket, Core::PacketDeleter> uniquePacket; // 단일 대상 send 
        std::shared_ptr<Core::IPacket> sharedPacket; // 여러 대상 send
        std::vector<std::shared_ptr<Core::IPacket>> packetChunks;

        STOverlappedEx() {
            wsaBuf.reserve(Core::AOICellCount);
            originalBufs.reserve(Core::AOICellCount);
            packetChunks.reserve(Core::AOICellCount);
        }

        void Reset()
        {
            wsaOverlapped = {};
            totalBytes = 0;
            sentBytes = 0;
            clientSocket = INVALID_SOCKET;
            ownerContext = nullptr;
            sessionID = 0;

            wsaBuf.clear();
            originalBufs.clear();

            uniquePacket.reset();
            sharedPacket.reset();
            packetChunks.clear();
        }
    };

}
