#pragma once

#include <memory>
#include <cstdint>

#include "../Interface/IPacket.h"
#include "MessageTypes.h"
#include "../LoggerGlobal.h"

namespace Core {
    class PacketWriter;
    class Message;
    class INetworkSender;
    class LobbyZone;
    class SessionManager;
    class IZoneRequestQueue;

    class MessageQueueHandler {
        // 캐시 or db 요청 결과 처리
        PacketWriter* writer = nullptr;
        INetworkSender* networkSender = nullptr;
        LobbyZone* lobbyZone = nullptr;
        SessionManager* sessionManager = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;
        void Initialize(INetworkSender* sender, PacketWriter* w, LobbyZone* lo, SessionManager* s, IZoneRequestQueue& requests) noexcept {
            networkSender = sender;
            writer = w;
            lobbyZone = lo;
            sessionManager = s;
            zoneRequests = &requests;
        }
        bool IsReady() const {
            if (networkSender == nullptr) {
                sysLogger->LogError("mq handler", "networkSender not initialized");
                return false;
            }
            if (!zoneRequests || !sessionManager || !lobbyZone) {
                sysLogger->LogError("mq handler", "state routing not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("mq handler", "writer not initialized");
                return false;
            }
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
