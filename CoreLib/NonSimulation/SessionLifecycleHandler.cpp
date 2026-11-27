#include "pch.h"
#include "SessionLifecycleHandler.h"
#include "../World/SessionManager.h"
#include "../World/LobbyZone.h"
#include "../World/Interface/IZoneRequestQueue.h"
#include "../Cache/CacheCommandSender.h"
#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"

namespace Core {
    void SessionLifecycleHandler::Disconnect(uint64_t sessionID) {
        const auto request = sessionManager->BeginDisconnect(sessionID);
        if (!request)
            return;
        if (request->zoneID != 0) {
            if (!zoneRequests->TryEnqueueDisconnect(request->zoneID, sessionID, request->ownershipEpoch))
                errorLogger->LogError("non simulation handler", "could not queue zone disconnect", {{"sessionID", sessionID}, {"zoneID", request->zoneID}});
            return;
        }
        CharacterState character;
        const bool save = request->chatID != 0 && lobbyZone->EmigrateChar(sessionID, character);
        if (request->chatID == 0)
            lobbyZone->Disconnect(sessionID);
        if (!sessionManager->CompleteLobbyDisconnect(sessionID, request->ownershipEpoch))
            errorLogger->LogError("non simulation handler", "lobby disconnect metadata changed", {{"sessionID", sessionID}});
        if (save)
            cacheCommands->EnqueueCharacterStateUpdate(sessionID, character);
    }

    void SessionLifecycleHandler::ReturnToLobby(const std::shared_ptr<ZoneTransfer>& transfer) {
        const auto sessionID = transfer->character.sessionID;
        const auto epoch = lobbyZone->ReturnZoneTransfer(transfer);
        if (!epoch)
            return;

        responses->Enqueue(sessionID, writer->WriteZoneChangeLobby(*epoch));
    }

}
