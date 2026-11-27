#include "pch.h"
#include "ZoneRequestHandler.h"
#include "../World/LobbyZone.h"
#include "../World/SessionManager.h"
#include "../World/Interface/IZonePacketQueue.h"
#include "../World/Interface/IZoneRequestQueue.h"
#include "../Packet/PacketTypes.h"
#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"
#include "../Data/ItemData.h"
#include <utility>

namespace Core {
    void ZoneRequestHandler::Resync(IPacketView* p) {
        if (p->GetLength() == sizeof(PacketStruct<ZoneResyncBody>)) {
            const auto* body = parseBody<ZoneResyncBody>(p->GetPtr());
            const auto route = sessionManager->GetBootstrapRoute(p->GetSessionID(), body->ownershipEpoch);
            if (route)
                zoneRequests->TryEnqueueBootstrap(route->zoneID, p->GetSessionID(), route->ownershipEpoch, false);
        }
    }

    void ZoneRequestHandler::ZoneChange(IPacketView* p) {
        const auto session = p->GetSessionID();
        if (p->GetLength() == sizeof(PacketStruct<ZoneChangeBody>)) {
            const auto request = sessionManager->GetLobbyEntry(session, parseBody<ZoneChangeBody>(p->GetPtr())->op);
            if (request) {
                const auto destination = lobbyZone->FindEntryZone(session, request->characterID);
                if (destination != 0 && zoneRequests->TryEnqueueLobbyEntry(destination, session, request->ownershipEpoch))
                    return;
            }
        }
        responses->Enqueue(session, writer->WriteZoneChangeFailed());
    }

    void ZoneRequestHandler::RespawnToTown(IPacketView* p) {
        const auto session = p->GetSessionID();
        const bool validHeader = p->GetLength() == sizeof(PacketStruct<RespawnToTownBody>) && parseHeader(p->GetPtr())->magic == MAGIC &&
                                 parseHeader(p->GetPtr())->opcode == OP::RESPAWN_TO_TOWN && parseHeader(p->GetPtr())->length == p->GetLength() &&
                                 !IsSimulationFlag(parseHeader(p->GetPtr())->flags);
        if (validHeader) {
            const auto route = sessionManager->GetRespawnRoute(session, parseBody<RespawnToTownBody>(p->GetPtr())->ownershipEpoch);
            if (route && (route->alreadyPending || zoneRequests->TryEnqueueRespawn(route->zoneID, session, route->ownershipEpoch)))
                return;
        }
        responses->Enqueue(session, writer->WriteZoneChangeFailed());
    }

    void ZoneRequestHandler::ForwardZoneRequest(std::unique_ptr<IPacketView, PacketViewDeleter> packet) {
        const auto sessionID = packet->GetSessionID();
        const auto zoneID = sessionManager->GetZoneID(sessionID);
        const auto opcode = packet->GetOpcode();
        if (opcode == OP::ITEM_USE) {
            const auto request = *parseBody<ItemUseRequestBody>(packet->GetPtr());
            if (zonePackets->TryEnqueuePacket(zoneID, std::move(packet)))
                return;
            const ItemUseResponseBody result{request.requestID, request.itemID, 6, 0, 0};
            errorLogger->LogWarn("non simulation handler", "item request queue full", {{"sessionID", sessionID}, {"zoneID", zoneID}});
            responses->Enqueue(sessionID, writer->WriteItemUseResponse(result));
            return;
        }
        uint64_t requestID, epoch;
        uint32_t itemID = 0;
        uint8_t slot = 0;
        const bool write = opcode == OP::EQUIPMENT_SET;
        if (write) {
            const auto request = *parseBody<EquipmentSetRequestBody>(packet->GetPtr());
            if (!Data::IsValidEquipmentSlot(static_cast<Data::EquipmentSlot>(request.slot)))
                return;
            requestID = request.requestID;
            epoch = request.ownershipEpoch;
            slot = request.slot;
            itemID = request.itemID;
        } else {
            const auto request = *parseBody<EquipmentGetRequestBody>(packet->GetPtr());
            requestID = request.requestID;
            epoch = request.ownershipEpoch;
        }
        EquipmentResponseBody result{requestID, epoch, static_cast<uint8_t>(write ? 1 : 0), static_cast<uint8_t>(EquipmentStatus::Invalid), slot};
        const auto check = sessionManager->CheckEquipment(sessionID, zoneID, requestID, epoch, write, slot, itemID, result);
        if (check == SessionRequestStatus::Ready) {
            if (zonePackets->TryEnqueuePacket(zoneID, std::move(packet)))
                return;
            result.status = static_cast<uint8_t>(EquipmentStatus::Busy);
            errorLogger->LogWarn("non simulation handler", "equipment request queue full", {{"sessionID", sessionID}, {"zoneID", zoneID}});
        } else if (check == SessionRequestStatus::StaleOwnership)
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleOwnership);
        else if (check == SessionRequestStatus::StaleRequest)
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleRequest);
        responses->Enqueue(sessionID, writer->WriteEquipmentResponse(result));
    }
}
