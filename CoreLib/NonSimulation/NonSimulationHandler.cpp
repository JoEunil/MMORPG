#include "pch.h"
#include "NonSimulationHandler.h"
#include "SessionLifecycleHandler.h"
#include "WorldEntryHandler.h"
#include "BazaarRequestHandler.h"
#include "ZoneRequestHandler.h"
#include "PlayerDataRequestHandler.h"
#include "ChatRequestHandler.h"
#include "../Packet/PacketTypes.h"
#include "../LoggerGlobal.h"
#include <utility>

namespace Core {
    void NonSimulationHandler::Process(NonSimulationEvent& event) {
        std::visit(EventVisitor{*this}, event);
    }

    void NonSimulationHandler::EventVisitor::operator()(NonSimulationWorkEvent& event) const {
        handler.Process(std::move(event.work));
        event.work.reset();
    }

    void NonSimulationHandler::EventVisitor::operator()(NonSimulationDisconnectEvent& event) const {
        handler.lifecycle->Disconnect(event.sessionID);
    }

    void NonSimulationHandler::EventVisitor::operator()(NonSimulationLobbyReturnEvent& event) const {
        handler.lifecycle->ReturnToLobby(event.transfer);
        event.transfer.reset();
    }

    void NonSimulationHandler::Initialize(WorldEntryHandler& worldEntryHandler, BazaarRequestHandler& bazaarHandler, ZoneRequestHandler& zoneHandler, PlayerDataRequestHandler& playerDataHandler, ChatRequestHandler& chatRequestsHandler, SessionLifecycleHandler& sessionLifecycle) {
        worldEntry = &worldEntryHandler;
        bazaar = &bazaarHandler;
        zone = &zoneHandler;
        playerData = &playerDataHandler;
        chatRequests = &chatRequestsHandler;
        lifecycle = &sessionLifecycle;
    }

    bool NonSimulationHandler::IsReady() const {
        if (worldEntry == nullptr) {
            sysLogger->LogError("non simulation handler", "worldEntry handler not initialized");
            return false;
        }
        if (bazaar == nullptr) {
            sysLogger->LogError("non simulation handler", "bazaar handler not initialized");
            return false;
        }
        if (zone == nullptr) {
            sysLogger->LogError("non simulation handler", "zone handler not initialized");
            return false;
        }
        if (playerData == nullptr) {
            sysLogger->LogError("non simulation handler", "playerData handler not initialized");
            return false;
        }
        if (chatRequests == nullptr) {
            sysLogger->LogError("non simulation handler", "chatRequests handler not initialized");
            return false;
        }
        if (lifecycle == nullptr) {
            sysLogger->LogError("non simulation handler", "lifecycle handler not initialized");
            return false;
        }
        return true;
    }

    void NonSimulationHandler::Process(std::unique_ptr<IPacketView, PacketViewDeleter> packet) {
        if (!packet)
            return;
        const auto opcode = packet->GetOpcode();
        if (opcode == OP::ITEM_USE || opcode == OP::EQUIPMENT_GET || opcode == OP::EQUIPMENT_SET) {
            zone->ForwardZoneRequest(std::move(packet));
            return;
        }
        auto* p = packet.get();
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode()) {
        case OP::AUTH:
            worldEntry->CheckSession(p);
            break;
        case OP::CHARACTER_LIST:
            worldEntry->GetCharacterList(p);
            break;
        case OP::ENTER_WORLD:
            worldEntry->GetCharacterState(p);
            break;
        case OP::CHAT:
            if (!chatRequests->Chat(p)) {
                NonSimulationEvent disconnectEvent = NonSimulationDisconnectEvent{p->GetSessionID()};
                Process(disconnectEvent);
            }
            break;
        case OP::ZONE_CHANGE:
            zone->ZoneChange(p);
            break;
        case OP::ZONE_RESYNC:
            zone->Resync(p);
            break;
        case OP::RESPAWN_TO_TOWN:
            zone->RespawnToTown(p);
            break;
        case OP::INVENTORY_REQ:
            playerData->GetInventory(p);
            break;
        case OP::BAZAAR_BUY:
            bazaar->BazaarBuy(p);
            break;
        case OP::BAZAAR_OUTBOX_LIST:
            bazaar->BazaarOutboxList(p);
            break;
        case OP::BAZAAR_CHECK_OUTBOX:
            bazaar->BazaarCheckOutbox(p);
            break;
        case OP::BAZAAR_CANCEL_LISTING:
            bazaar->BazaarCancelListing(p);
            break;
        case OP::BAZAAR_CLAIM:
            bazaar->BazaarClaim(p);
            break;
        case OP::BAZAAR_MY_LIST:
            bazaar->BazaarMyList(p);
            break;
        case OP::BAZAAR_SEARCH:
            bazaar->BazaarSearch(p);
            break;
        case OP::BAZAAR_REGISTER:
            bazaar->BazaarRegister(p);
            break;
        case OP::WALLET_REQUEST:
            playerData->WalletRequest(p);
            break;
        case OP::PROFILE_BATCH:
            if (!playerData->GetProfileBatch(p)) {
                NonSimulationEvent disconnectEvent = NonSimulationDisconnectEvent{p->GetSessionID()};
                Process(disconnectEvent);
            }
            break;
        default:
            errorLogger->LogInfo("non zone handler", "UnDefined OPCODE", {{"opcode", p->GetOpcode()}, {"header opcode", h->opcode}, {"sessionId", p->GetSessionID()}});
            break;
        }
    }

}
