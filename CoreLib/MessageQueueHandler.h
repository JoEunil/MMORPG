#pragma once

#include <memory>
#include <cstdint>

#include "IPacket.h"
#include "MessageTypes.h"
#include "LoggerGlobal.h"

namespace Core {
    class PacketWriter;
    class Message;
    class MessagePool;
    class INetworkSender;
    class LobbyZone;
    class StateManager;

    class MessageQueueHandler {
        // 캐시 or db 요청 결과 처리
        PacketWriter* writer = nullptr;
        INetworkSender* networkSender = nullptr;
        LobbyZone* lobbyZone = nullptr;
        MessagePool* messagePool = nullptr;
        StateManager* stateManager = nullptr;
        void Initialize(INetworkSender* sender, PacketWriter* w, LobbyZone* lo, MessagePool* msgPool, StateManager* s) noexcept {
            networkSender = sender;
            writer = w;
            lobbyZone = lo;
            messagePool = msgPool;
            stateManager = s;
        }
        bool IsReady() {
            if (networkSender == nullptr || writer == nullptr)
                return false;
            return true;
        }

        enum class ResponseScope { Session, FieldSession, FieldCharacter };

        template<typename T>
        using ResponseWriter = std::unique_ptr<IPacket, PacketDeleter>(PacketWriter::*)(const T*);

        template<typename T>
        void SendResponse(uint64_t sessionID, const T* body, ResponseWriter<T> write);

        template<ResponseScope Scope = ResponseScope::Session, typename T>
        void DispatchResponse(Message& msg, uint64_t sessionID, ResponseWriter<T> write);

        bool IsCurrentFieldCharacter(uint64_t sessionID, uint64_t characterID);
        void CharacterStateResponse(uint64_t sessionID, const MsgCharacterStateResBody* body);
        void GoldDepositResponse(uint64_t sessionID, const MsgCurrencyDepositResBody* body);
        void ProfileRenameResponse(uint64_t sessionID, const MsgProfileRenameResBody* body);


        friend class Initializer;
    public:
        void Process(Message* m);
    };
}
