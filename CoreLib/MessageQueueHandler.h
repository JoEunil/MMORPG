#pragma once

#include <memory>
#include <cstdint>

#include "MessageTypes.h"

namespace Core {
    class PacketWriter;
    class Message;
    class MessagePool;
    class IIOCP;
    class LobbyZone;
    
    class MessageQueueHandler {
        // 캐시 or db 요청 결과 처리
        PacketWriter* writer;
        IIOCP* iocp;
        LobbyZone* lobbyZone;
        MessagePool* messagePool;
        void Initialize(IIOCP* i, PacketWriter* w, LobbyZone* lo, MessagePool* msgPool) noexcept {
            iocp = i;
            writer = w;
            lobbyZone = lo;
            messagePool = msgPool;
        }
        bool IsReady() {
            if (iocp == nullptr || writer == nullptr)
                return false;
            return true;
        }

        void CharacterListResponse(uint64_t sessionID, MsgCharacterListResBody* body);
        void CharacterStateResponse(uint64_t sessionID, MsgCharacterStateResBody* body);
        void InventoryResponse(uint64_t sessionID, MsgInventoryResBody* body);
        void InventoryUpdateResponse(uint64_t sessionID, MsgInventoryUpdateResBody* body);
        
        friend class Initializer;
    public:
        void Process(Message* m);
    };
}
