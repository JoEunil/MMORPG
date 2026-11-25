#pragma once

#include <memory>
#include <cstdint>

#include "../Interface/IPacket.h"
#include "MessageTypes.h"
#include "../LoggerGlobal.h"

namespace Core {
    class IMessage;
    class IServerResponseQueue;
    class PacketWriter;
    class LobbyZone;
    class SessionManager;
    class IZoneRequestQueue;

    class MessageQueueHandler {
        // 캐시 or db 요청 결과 처리
        LobbyZone* lobbyZone = nullptr;
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        SessionManager* sessionManager = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;
        void Initialize(IServerResponseQueue& responseQueue, PacketWriter& packetWriter, LobbyZone* lo, SessionManager* s, IZoneRequestQueue& requests) noexcept {
            responses = &responseQueue;
            writer = &packetWriter;
            lobbyZone = lo;
            sessionManager = s;
            zoneRequests = &requests;
        }
        bool IsReady() const {
            if (writer == nullptr) {
                sysLogger->LogError("mq handler", "packet writer not initialized");
                return false;
            }
            if (responses == nullptr) {
                sysLogger->LogError("mq handler", "response queue not initialized");
                return false;
            }
            if (zoneRequests == nullptr) {
                sysLogger->LogError("mq handler", "zone request queue not initialized");
                return false;
            }
            if (sessionManager == nullptr) {
                sysLogger->LogError("mq handler", "session manager not initialized");
                return false;
            }
            if (lobbyZone == nullptr) {
                sysLogger->LogError("mq handler", "lobby zone not initialized");
                return false;
            }
            return true;
        }

        enum class ResponseScope { Session, FieldSession, FieldCharacter };

        template<typename T>
        using ResponseWriter = std::unique_ptr<IPacket, PacketDeleter>(PacketWriter::*)(const T*);

        template<ResponseScope Scope = ResponseScope::Session, typename T>
        void DispatchResponse(IMessage& msg, uint64_t sessionID, ResponseWriter<T> write);

        bool IsCurrentFieldCharacter(uint64_t sessionID, uint64_t characterID);
        void CharacterStateResponse(uint64_t sessionID, const MsgCharacterStateResBody* body);
        void GoldDepositResponse(uint64_t sessionID, const MsgCurrencyDepositResBody* body);
        void ProfileRenameResponse(uint64_t sessionID, const MsgProfileRenameResBody* body);


        friend class Initializer;
    public:
        void Process(IMessage* m);
    };
}
