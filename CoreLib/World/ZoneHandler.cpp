#include "pch.h"
#include "ZoneHandler.h"
#include "ZoneEventHandler.h"

#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"
#include "ZoneState.h"
#include "../LoggerGlobal.h"
#include <cmath>

namespace Core {
    size_t ZoneHandler::ProcessEvent(ZoneState& zone, ZoneEvent& event) {
        return std::visit(EventVisitor{*this, zone}, event);
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZonePacketEvent& event) const {
        handler.Process(zone, event.packet.get());
        return 1;
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZoneTransferEvent& event) const {
        handler.events->ProcessZoneTransfer(zone, event.transfer, false);
        return 0;
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZoneTransferRejectedEvent& event) const {
        handler.events->ProcessZoneTransfer(zone, event.transfer, true);
        return 0;
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZoneProfileVersionEvent& event) const {
        handler.events->ApplyProfileVersion(zone, event.update);
        return 0;
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZoneBootstrapEvent& event) const {
        handler.events->ProcessBootstrap(zone, event);
        return 0;
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZoneLobbyEntryEvent& event) const {
        handler.events->ProcessLobbyEntry(zone, event);
        return 0;
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZoneRespawnEvent& event) const {
        handler.events->ProcessRespawn(zone, event);
        return 0;
    }

    size_t ZoneHandler::EventVisitor::operator()(const ZoneDisconnectEvent& event) const {
        handler.events->ProcessDisconnect(zone, event);
        return 0;
    }

    void ZoneHandler::ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID) {
        if (!std::isfinite(action.aimAngle) || action.aimAngle < 0 || action.aimAngle >= 360)
            return;
        std::optional<ZoneRequestRoute> resync;
        if (!sessionManager->CheckZoneAction({sessionID, zone.GetZoneID(), action.ownershipEpoch}, resync)) {
            if (resync)
                zoneRequests->TryEnqueueBootstrap({sessionID, resync->zoneID, resync->ownershipEpoch}, true);
            return;
        }
        zone.ApplyAction(sessionID, action, *zoneTransfer);
    }

    ParryResponseBody ZoneHandler::ProcessParry(ZoneState& zone, uint64_t sessionID, const ParryRequestBody& request) {
        if (const auto rejection = sessionManager->GetParryRejection(sessionID, zone.GetZoneID(), request))
            return {request.requestID, request.ownershipEpoch, static_cast<uint8_t>(*rejection), 0, 0};
        return zone.Parry(sessionID, request);
    }

    void ZoneHandler::ProcessItemUse(ZoneState& zone, IPacketView* p) {
        const auto request = *parseBody<ItemUseRequestBody>(p->GetPtr());
        const auto applied = zone.UseItem(p->GetSessionID(), request);
        const auto result = applied.value_or(ItemUseResponseBody{request.requestID, request.itemID, 0, 0, 0});
        responses->Enqueue(p->GetSessionID(), writer->WriteItemUseResponse(result));
    }

    void ZoneHandler::ProcessEquipment(ZoneState& zone, IPacketView* p) {
        const auto zoneID = zone.GetZoneID();
        const bool write = p->GetOpcode() == OP::EQUIPMENT_SET;
        uint64_t requestID, epoch;
        uint8_t slot = 0;
        uint32_t itemID = 0;
        if (write) {
            const auto request = *parseBody<EquipmentSetRequestBody>(p->GetPtr());
            requestID = request.requestID;
            epoch = request.ownershipEpoch;
            slot = request.slot;
            itemID = request.itemID;
        } else {
            const auto request = *parseBody<EquipmentGetRequestBody>(p->GetPtr());
            requestID = request.requestID;
            epoch = request.ownershipEpoch;
        }
        EquipmentResponseBody result{requestID, epoch, static_cast<uint8_t>(write ? 1 : 0), static_cast<uint8_t>(EquipmentStatus::Invalid), slot};
        const auto check = sessionManager->CheckEquipment(p->GetSessionID(), zoneID, requestID, epoch, write, slot, itemID, result);
        if (check == SessionRequestStatus::Ready) {
            if (const auto applied = zone.Equipment(p->GetSessionID(), requestID, epoch, write, slot, itemID)) {
                result = *applied;
                if (write)
                    sessionManager->RememberEquipment(p->GetSessionID(), zoneID, itemID, result);
            }
        } else if (check == SessionRequestStatus::StaleOwnership)
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleOwnership);
        else if (check == SessionRequestStatus::StaleRequest)
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleRequest);
        responses->Enqueue(p->GetSessionID(), writer->WriteEquipmentResponse(result));
    }

    void ZoneHandler::Process(ZoneState& zone, IPacketView* p) {
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode()) {
        case OP::ITEM_USE:
            ProcessItemUse(zone, p);
            break;
        case OP::EQUIPMENT_GET:
        case OP::EQUIPMENT_SET:
            ProcessEquipment(zone, p);
            break;
        case OP::PARRY: {
            const auto result = ProcessParry(zone, p->GetSessionID(), *parseBody<ParryRequestBody>(p->GetPtr()));
            responses->Enqueue(p->GetSessionID(), writer->WriteParryResponse(result));
            break;
        }
        case OP::ACTION:
            ProcessAction(zone, *parseBody<ActionRequestBody>(p->GetPtr()), p->GetSessionID());
            break;
        default:
            errorLogger->LogInfo("zone Handler", "UnDefined OPCODE", {{"zone", zone.GetZoneID()}, {"opcode", p->GetOpcode()}, {"session", p->GetSessionID()}});
            break;
        }
    }

    void ZoneHandler::ProcessDisconnect(ZoneState& zone, const ZoneDisconnectEvent& request) {
        events->ProcessDisconnect(zone, request);
    }

    void ZoneHandler::SaveRemainingCharacters(ZoneState& zone) {
        events->SaveRemainingCharacters(zone);
    }
}
