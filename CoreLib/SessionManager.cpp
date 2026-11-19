#include "pch.h"
#include "SessionManager.h"
#include "CacheCommandSender.h"
#include "ChatThreadPool.h"
#include "LobbyZone.h"
#include "PacketWriter.h"
#include "ItemData.h"
#include "IZoneRequestQueue.h"
#include "ZoneEventRouter.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace Core {
    ItemUseResponseBody SessionManager::UseItem(uint64_t sessionID, const ItemUseRequestBody& request) {
        ItemUseResponseBody result{request.requestID, request.ownershipEpoch, request.itemID, 0, 0, 0};
        if (request.requestID == 0 || request.itemID == 0) return result;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated
            || !session->second.authCompleted || session->second.characterID == 0) return result;
        auto& saved = session->second.lastItemUse;
        if (request.requestID == saved.requestID) {
            if (request.ownershipEpoch == saved.ownershipEpoch && request.itemID == saved.itemID) return saved;
            return result;
        }
        if (request.requestID < saved.requestID || request.ownershipEpoch != session->second.ownershipEpoch) {
            result.status = 7;
            return result;
        }
        auto zone = zoneRouter->GetZone(session->second.zoneID);
        if (!zone) return result;
        std::unique_lock zoneLock(zone->m_mutex);
        const auto found = zone->m_sessionToIndex.find(sessionID);
        if (found == zone->m_sessionToIndex.end()) return result;
        auto& character = zone->m_chars[found->second];
        result.cooldownRemainingTicks = character.potionCooldownTicks;
        const auto item = Data::itemMap.find(request.itemID);
        if (item == Data::itemMap.end() || (item->second.hpRecovery == 0 && item->second.mpRecovery == 0)) result.status = 9;
        else if (character.hp <= 0) result.status = 2;
        else if (item->second.hpRecovery != 0 ? character.hp >= character.maxHp : character.mp >= character.maxMp) result.status = 3;
        else if (character.potionCooldownTicks != 0) result.status = 4;
        else if (!inventory) result.status = 8;
        else {
            const auto consumed = inventory->ConsumeItem(character.characterID, request.itemID);
            result.remainingQuantity = consumed.remainingQuantity;
            switch (consumed.status) {
            case ItemOperationStatus::Success:
                if (item->second.hpRecovery != 0)
                    character.hp = static_cast<int>((std::min)(static_cast<int64_t>(character.maxHp),
                        static_cast<int64_t>(character.hp) + item->second.hpRecovery));
                if (item->second.mpRecovery != 0)
                    character.mp = static_cast<int>((std::min)(static_cast<int64_t>(character.maxMp),
                        static_cast<int64_t>(character.mp) + item->second.mpRecovery));
                character.potionCooldownTicks = item->second.cooldownTicks;
                character.dirtyBit |= 3;
                zone->m_cells[character.cellY][character.cellX].dirtyChar.push_back(character.zoneInternalID);
                result.status = 1;
                result.cooldownRemainingTicks = character.potionCooldownTicks;
                break;
            case ItemOperationStatus::Missing: result.status = 5; break;
            case ItemOperationStatus::Busy: result.status = 6; break;
            default: result.status = 8; break;
            }
        }
        saved = result;
        return result;
    }

    EquipmentResponseBody SessionManager::Equipment(uint64_t sessionID, uint64_t requestID, uint64_t ownershipEpoch, bool write, uint8_t slot, uint32_t itemID) {
        EquipmentResponseBody result{ requestID, ownershipEpoch, static_cast<uint8_t>(write ? 1 : 0), static_cast<uint8_t>(EquipmentStatus::Invalid), slot, 0, 0, 0, 0, 0, 0, 0 };
        if (requestID == 0 || ownershipEpoch == 0) return result;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated || !session->second.authCompleted
            || session->second.characterID == 0) return result;
        if (session->second.zoneID == 0 || ownershipEpoch != session->second.ownershipEpoch) {
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleOwnership);
            return result;
        }
        auto& previous = session->second.lastEquipmentSet;
        if (write && requestID <= previous.requestID) {
            if (requestID == previous.requestID && ownershipEpoch == previous.ownershipEpoch
                && slot == previous.slot && itemID == session->second.lastEquipmentItemID) return previous;
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleRequest);
            return result;
        }
        auto* zone = zoneRouter->GetZone(session->second.zoneID);
        if (zone == nullptr) return result;
        std::lock_guard zoneLock(zone->m_mutex);
        auto member = zone->m_sessionToIndex.find(sessionID);
        if (member == zone->m_sessionToIndex.end()) return result;
        auto& character = zone->m_chars[member->second];
        if (slot > 2 || (!write && slot != 0) || (write && itemID != 0 && itemID != 3u + slot)) result.status = static_cast<uint8_t>(EquipmentStatus::Unsupported);
        else if (write && character.hp <= 0) result.status = static_cast<uint8_t>(EquipmentStatus::Dead);
        else if (inventory == nullptr) result.status = static_cast<uint8_t>(EquipmentStatus::StorageFailure);
        else {
            const auto state = write ? inventory->EquipItem(character.characterID, slot, itemID) : inventory->GetEquipmentState(character.characterID);
            switch (state.status) {
            case EquipmentOperationStatus::Success:
                if ((state.weaponID != 0 && state.weaponID != 3) || (state.shieldID != 0 && state.shieldID != 4)
                    || (state.bodyID != 0 && state.bodyID != 5)
                    || (write && (slot == 0 ? state.weaponID : slot == 1 ? state.shieldID : state.bodyID) != itemID)) {
                    result.status = static_cast<uint8_t>(EquipmentStatus::StorageFailure);
                    break;
                }
                character.equippedWeaponItemID = state.weaponID;
                character.equippedShieldItemID = state.shieldID;
                character.equippedBodyItemID = state.bodyID;
                result.status = static_cast<uint8_t>(EquipmentStatus::Success);
                break;
            case EquipmentOperationStatus::Missing:
                result.status = static_cast<uint8_t>(EquipmentStatus::Missing);
                break;
            case EquipmentOperationStatus::Busy: result.status = static_cast<uint8_t>(EquipmentStatus::Busy); break;
            case EquipmentOperationStatus::Unsupported: result.status = static_cast<uint8_t>(EquipmentStatus::Unsupported); break;
            default: result.status = static_cast<uint8_t>(EquipmentStatus::StorageFailure); break;
            }
        }
        if (result.status == static_cast<uint8_t>(EquipmentStatus::Success)) {
            result.weaponID = character.equippedWeaponItemID;
            result.shieldID = character.equippedShieldItemID;
            result.bodyID = character.equippedBodyItemID;
            result.defensePercent = character.DefensePercent();
            result.baseAttack = character.attack;
            result.bonusAttack = character.WeaponAttackBonus();
            result.effectiveAttack = character.EffectiveAttack();
        }
        if (write) {
            previous = result;
            session->second.lastEquipmentItemID = itemID;
        }
        return result;
    }

    void SessionManager::GrantMonsterReward(const CharacterState& character, uint64_t gold) {
        const auto sessionID = character.sessionID;
        if (!cacheCommands->EnqueueGoldDeposit(sessionID, character.characterID, gold)) {
            errorLogger->LogError("monster reward", "gold deposit queue rejected", "characterID", character.characterID);
            auto packet = ZoneState::writer->WriteGoldReward(0);
            if (packet) networkSender->SendDataUnique(sessionID, std::move(packet));
        }
    }

    bool SessionManager::QueueLobbyEntry(uint64_t sessionID, uint8_t op) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (op != ZONE_CHANGE::ENTER || session == shard.sessionMap.end())
            return false;
        const auto& data = session->second;
        if (!data.authenticated || !data.authCompleted || data.characterID == INVALID_CHARACTER_ID
            || data.zoneID != 0 || data.ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return false;

        const uint16_t destination = lobbyZone->FindEntryZone(sessionID, data.characterID);
        return destination != 0 && zoneRequests->TryEnqueueLobbyEntry(destination, sessionID, data.ownershipEpoch);
    }

    bool SessionManager::QueueRespawnToTown(uint64_t sessionID, uint64_t ownershipEpoch) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end())
            return false;
        const auto& data = session->second;
        if (!data.authenticated || !data.authCompleted || data.userID == 0
            || data.characterID == INVALID_CHARACTER_ID || data.zoneID == 0 || data.zoneID > ZONE_COUNT
            || ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return false;
        if (data.pendingZoneTransfer)
            return data.pendingZoneTransfer->IsRespawn() && ownershipEpoch == data.ownershipEpoch;

        const bool replay = data.zoneID == TOWN_ZONE_ID && data.lastRespawnRequestEpoch == ownershipEpoch
            && data.ownershipEpoch == ownershipEpoch + 1;
        if (data.ownershipEpoch != ownershipEpoch && !replay)
            return false;

        return zoneRequests->TryEnqueueRespawn(data.zoneID, sessionID, ownershipEpoch);
    }

    void SessionManager::UpdateZoneOwnershipLocked(SessionData& session, uint16_t destinationID) {
        session.zoneID = destinationID;
        ++session.ownershipEpoch;
    }

    bool SessionManager::MatchesZoneTransfer(const SessionData* session, const std::shared_ptr<ZoneTransfer>& transfer) {
        return session && session->pendingZoneTransfer == transfer
            && session->zoneID == transfer->sourceZoneID && session->ownershipEpoch == transfer->ownershipEpoch;
    }

    void SessionManager::JoinZoneChatLocked(uint64_t sessionID, uint16_t zoneID, SessionData& session,
        std::optional<uint32_t> lobbyProfile) {
        if (lobbyProfile && session.chatID == 0)
            session.chatID = chat->AddChatSession(sessionID, zoneID, *lobbyProfile);
        chat->EnqueueZoneJoin(sessionID, zoneID);
    }

    void SessionManager::LeaveZoneChatLocked(uint64_t sessionID, uint16_t zoneID) {
        chat->EnqueueZoneLeave(sessionID, zoneID);
    }

    bool SessionManager::QueueBootstrap(uint64_t sessionID, uint64_t ownershipEpoch) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        return session != shard.sessionMap.end() && session->second.ownershipEpoch == ownershipEpoch
            && QueueBootstrapLocked(sessionID, session->second, false);
    }

    bool SessionManager::QueueBootstrapLocked(uint64_t sessionID, const SessionData& session, bool resendZoneChange) {
        if (session.zoneID == 0 || session.ownershipEpoch == 0 || session.pendingZoneTransfer)
            return false;
        return zoneRequests->TryEnqueueBootstrap(session.zoneID, sessionID, session.ownershipEpoch, resendZoneChange);
    }

    uint64_t SessionManager::NextBootstrapIDLocked(SessionData& session) {
        if (session.bootstrapSequence == (std::numeric_limits<uint64_t>::max)())
            return 0;
        return ++session.bootstrapSequence;
    }

    void SessionManager::CleanUp() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
        for (auto& shard : m_shards)
        {
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            for (auto& [session, data] : shard.sessionMap)
            {
                if (const auto& pending = data.pendingZoneTransfer) {
                    EnqueueCharacterStateUpdate(session, pending->character);
                    continue;
                }
                CharacterState temp;
                if (data.zoneID == 0) {
                    if (lobbyZone->EmigrateChar(session, temp) == false) {
                        continue;
                    }
                }
                else {
                    auto* zone = zoneRouter->GetZone(data.zoneID);
                    if (zone == nullptr) {
                        gameLogger->LogInfo("core session manager", "state not found", "sessionID", session, "zoneID", data.zoneID);
                        continue;
                    }
                    if (!zone->EmigrateChar(session, temp)) {
                        continue;
                    }
                }
                EnqueueCharacterStateUpdate(session, temp);
            }
            shard.sessionMap.clear();
        }
    }

    void SessionManager::Disconnect(uint64_t sessionID) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock<std::shared_mutex> lock(shard.smutex);
        auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end())
        {
            return;
        }
        const auto zoneID = it->second.zoneID;
        if (zoneID == 0 && it->second.chatID == 0) {
            lobbyZone->Disconnect(sessionID);
            shard.sessionMap.erase(it);
            return;
        }

        CharacterState temp;
        const auto pending = it->second.pendingZoneTransfer;
        if (zoneID == 0) {
            if (!lobbyZone->EmigrateChar(sessionID, temp))
                return;
        }
        else if (!pending) {
            auto* zone = zoneRouter->GetZone(zoneID);
            if (zone == nullptr) {
                gameLogger->LogInfo("core session manager", "state not found", "sessionID", sessionID, "zoneID", zoneID);
                return;
            }
            if (!zone->EmigrateChar(sessionID, temp))
                return;
        }
        shard.sessionMap.erase(it);
        if (zoneID != 0)
            chat->EnqueueZoneLeave(sessionID, zoneID);
        chat->DeleteChatSession(sessionID, zoneID);
        lock.unlock();
        gameLogger->LogInfo("core session manager", "Enqueue Disconnect", "sessionID", sessionID);
        EnqueueCharacterStateUpdate(sessionID, pending ? pending->character : temp);
    }

    void SessionManager::QueueProfileVersionUpdate(const ProfileVersionUpdate& update) {
        auto& shard = m_shards[update.sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        const auto session = shard.sessionMap.find(update.sessionID);
        if (session != shard.sessionMap.end())
            RouteProfileVersionLocked(session->second, update);
    }

    void SessionManager::RouteProfileVersionLocked(const SessionData& session, const ProfileVersionUpdate& update) {
        if (const auto& pending = session.pendingZoneTransfer) {
            pending->character.UpdateProfileVersion(update.profileID, update.version);
            return;
        }

        if (session.zoneID == 0) {
            lobbyZone->UpdateProfileVersion(update.sessionID, update.profileID, update.version);
            return;
        }

        if (!zoneRequests->TryEnqueueProfileVersion(session.zoneID, update)) {
            errorLogger->LogWarn("core session manager", "could not queue profile version update",
                "sessionID", update.sessionID, "zoneID", session.zoneID, "version", update.version);
        }
    }

    void SessionManager::EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) {
        if (!cacheCommands->EnqueueCharacterStateUpdate(sessionID, character)) {
            std::vector<std::byte> binary(sizeof(CharacterState));
            std::memcpy(binary.data(), &character, sizeof(CharacterState));
            errorLogger->LogError("core session manager", "Failed to enqueue character state update", "sessionID", sessionID, "character state", binary);
        }
    }
}
