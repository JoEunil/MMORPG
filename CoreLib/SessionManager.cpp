#include "pch.h"
#include "SessionManager.h"
#include "CacheCommandSender.h"
#include "ChatThreadPool.h"
#include "LobbyZone.h"
#include "PacketWriter.h"
#include "WorldMap.h"
#include "ItemData.h"
#include "ZoneEventRouter.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Core {
    static_assert(TOWN_ZONE_ID == 1 && TOWN_SPAWN_X >= 0 && TOWN_SPAWN_X <= ZONE_SIZE
        && TOWN_SPAWN_Y >= 0 && TOWN_SPAWN_Y <= ZONE_SIZE,
        "Town respawn must remain inside Zone 1.");

    ParryResponseBody SessionManager::Parry(uint64_t sessionID, uint16_t routeZoneID, const ParryRequestBody& request) {
        ParryResponseBody result{request.requestID, request.ownershipEpoch, static_cast<uint8_t>(ParryStatus::Invalid), 0, 0};
        if (request.requestID == 0 || request.ownershipEpoch == 0) return result;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated || !session->second.authCompleted) {
            result.status = static_cast<uint8_t>(ParryStatus::Unavailable);
            return result;
        }
        if (session->second.zoneID == 0 || session->second.zoneID != routeZoneID || session->second.ownershipEpoch != request.ownershipEpoch) {
            result.status = static_cast<uint8_t>(ParryStatus::StaleOwnership);
            return result;
        }
        auto* zone = zoneRouter->GetZone(routeZoneID);
        if (!zone) { result.status = static_cast<uint8_t>(ParryStatus::Unavailable); return result; }
        std::lock_guard zoneLock(zone->m_mutex);
        auto member = zone->m_sessionToIndex.find(sessionID);
        if (member == zone->m_sessionToIndex.end()) { result.status = static_cast<uint8_t>(ParryStatus::Unavailable); return result; }
        auto& character = zone->m_chars[member->second];
        auto& previous = session->second.lastParry;
        if (request.requestID < previous.requestID || (request.requestID == previous.requestID && request.ownershipEpoch != previous.ownershipEpoch)) {
            result.status = static_cast<uint8_t>(ParryStatus::StaleRequest);
            return result;
        }
        if (request.requestID == previous.requestID) {
            result.status = previous.status;
        } else {
            if (character.hp <= 0) result.status = static_cast<uint8_t>(ParryStatus::Dead);
            else if (character.parryCooldownTicks != 0) result.status = static_cast<uint8_t>(ParryStatus::Cooldown);
            else {
                character.parryWindowTicks = Data::parryWindowTicks;
                character.parryCooldownTicks = Data::parryCooldownTicks;
                result.status = static_cast<uint8_t>(ParryStatus::Accepted);
                zone->m_cells[character.cellY][character.cellX].actionResults.push_back({0, character.zoneInternalID, 0,
                    Data::parrySkillSlot, character.dir, character.x, character.y, Data::parrySkillID, 0,
                    character.dir == 0 ? 0.0f : character.dir == 1 ? 180.0f : character.dir == 2 ? 270.0f : 90.0f});
            }
            previous = result;
        }
        // Only successful/cooldown outcomes describe a usable current combat state.
        if (result.status == static_cast<uint8_t>(ParryStatus::Accepted) || result.status == static_cast<uint8_t>(ParryStatus::Cooldown)) {
            result.windowRemainingTicks = character.parryWindowTicks;
            result.cooldownRemainingTicks = character.parryCooldownTicks;
        }
        return result;
    }

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

    bool SessionManager::ChangeZone(uint64_t sessionID, uint8_t op, ZoneChangeResult& result) {
        result = {};
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated
            || !session->second.authCompleted || session->second.characterID == 0)
            return false;
        const uint16_t sourceID = session->second.zoneID;
        if (op != ZONE_CHANGE::ENTER || sourceID != 0
            || session->second.ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return false;

        if (sourceID == 0) {
            std::unique_lock lobbyLock(lobbyZone->m_mutex);
            auto loaded = lobbyZone->m_chars.find(sessionID);
            if (loaded == lobbyZone->m_chars.end()
                || loaded->second.characterID != session->second.characterID)
                return false;
            const uint16_t destinationID = loaded->second.lastZone == 0 ? TOWN_ZONE_ID : loaded->second.lastZone;
            auto destination = zoneRouter->GetZone(destinationID);
            if (destination == nullptr)
                return false;
            std::unique_lock destinationLock(destination->m_mutex);
            CharacterState incoming = loaded->second;
            if (!std::isfinite(incoming.x) || !std::isfinite(incoming.y))
                return false; // Corrupt coordinates are not a map-version relocation.
            if (incoming.x < destination->m_area.x_min || incoming.x > destination->m_area.x_max
                || incoming.y < destination->m_area.y_min || incoming.y > destination->m_area.y_max
                || !WorldMap::CanTraverse(incoming.x, incoming.y, incoming.x, incoming.y)) {
                // Saved coordinates can become invalid after a fixed-map revision. Relocate only
                // on lobby entry; live Zone transfers must still prove their existing position.
                incoming.x = destination->m_area.x_min + TRANSITION_BUFFER + 10.0f;
                incoming.y = destination->m_area.y_min + TRANSITION_BUFFER + 10.0f;
                if (!WorldMap::CanTraverse(incoming.x, incoming.y, incoming.x, incoming.y)) {
                    const auto& safeArea = destination->m_cells[0][0].area;
                    const int firstX = (std::max)(0, static_cast<int>(std::floor((safeArea.x_min - WorldMap::OriginX) / WorldMap::TileSize)));
                    const int firstY = (std::max)(0, static_cast<int>(std::floor((safeArea.y_min - WorldMap::OriginY) / WorldMap::TileSize)));
                    const int lastX = (std::min)(WorldMap::Width - 1, static_cast<int>(std::floor((safeArea.x_max - WorldMap::OriginX) / WorldMap::TileSize)));
                    const int lastY = (std::min)(WorldMap::Height - 1, static_cast<int>(std::floor((safeArea.y_max - WorldMap::OriginY) / WorldMap::TileSize)));
                    bool found = false;
                    for (int y = firstY; y <= lastY && !found; ++y) {
                        for (int x = firstX; x <= lastX; ++x) {
                            const float safeX = WorldMap::OriginX + (x + 0.5f) * WorldMap::TileSize;
                            const float safeY = WorldMap::OriginY + (y + 0.5f) * WorldMap::TileSize;
                            if (safeX < safeArea.x_min || safeX > safeArea.x_max
                                || safeY < safeArea.y_min || safeY > safeArea.y_max
                                || !WorldMap::CanTraverse(safeX, safeY, safeX, safeY))
                                continue;
                            incoming.x = safeX;
                            incoming.y = safeY;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        return false; // The original lobby row and session are still intact.
                }
            }
            // Immigration copies and prepares the destination before removing the lobby row.
            if (destination->ImmigrateCharLocked(sessionID, incoming) == INVALID_ZONE_INTERNAL_ID)
                return false;
            const auto& entered = destination->m_chars.back();
            ++session->second.ownershipEpoch;
            result = {destinationID, 0, entered.zoneInternalID, entered.x, entered.y, destination->m_generation};
            result.ownershipEpoch = session->second.ownershipEpoch;
            lobbyZone->m_chars.erase(loaded);
            session->second.zoneID = destinationID;
            result.chatID = chat->AddChatSession(sessionID, destinationID, entered.profileId);
            session->second.chatID = result.chatID;
            chat->EnqueueZoneJoin(sessionID, destinationID);
            return true;
        }

        return false;
    }

    bool SessionManager::RespawnToTown(uint64_t sessionID, const RespawnToTownBody& request, ZoneChangeResult& result) {
        result = {};
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated || !session->second.authCompleted
            || session->second.userID == 0 || session->second.characterID == 0
            || session->second.zoneID == 0 || session->second.zoneID > ZONE_COUNT
            || !WorldMap::CanTraverse(TOWN_SPAWN_X, TOWN_SPAWN_Y, TOWN_SPAWN_X, TOWN_SPAWN_Y))
            return false;

        const uint16_t sourceID = session->second.zoneID;
        auto source = zoneRouter->GetZone(sourceID);
        auto town = zoneRouter->GetZone(TOWN_ZONE_ID);
        if (source == nullptr || town == nullptr)
            return false;

        // If the commit succeeded but its ACK was not observed, replay only that same
        // request while the session still owns the untouched town respawn state.
        if (session->second.ownershipEpoch != request.ownershipEpoch) {
            if (request.ownershipEpoch == (std::numeric_limits<uint64_t>::max)()
                || request.ownershipEpoch + 1 != session->second.ownershipEpoch
                || session->second.lastRespawnRequestEpoch != request.ownershipEpoch
                || sourceID != TOWN_ZONE_ID)
                return false;
            std::lock_guard zoneLock(town->m_mutex);
            auto member = town->m_sessionToIndex.find(sessionID);
            if (member == town->m_sessionToIndex.end()) return false;
            const auto& character = town->m_chars[member->second];
            if (character.characterID != session->second.characterID || character.hp <= 0
                || character.hp != character.maxHp || character.mp != character.maxMp
                || character.x != TOWN_SPAWN_X || character.y != TOWN_SPAWN_Y)
                return false;
            result = {TOWN_ZONE_ID, session->second.chatID, character.zoneInternalID,
                character.x, character.y, town->m_generation, session->second.ownershipEpoch};
            return true;
        }
        if (session->second.ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return false;

        auto clearPlayerEffects = [sessionID](ZoneState& zone, uint32_t internalID) {
            for (auto& row : zone.m_cells) {
                for (auto& cell : row) {
                    std::erase_if(cell.activeSkills, [sessionID](const ActiveSkill& skill) {
                        return skill.casterType == 0 && skill.sessionId == sessionID;
                    });
                    std::erase_if(cell.actionResults, [internalID](const ActionResult& action) {
                        return action.casterType == 0 && action.zoneInternalId == internalID;
                    });
                    std::erase(cell.dirtyChar, internalID);
                }
            }
        };

        if (sourceID == TOWN_ZONE_ID) {
            std::lock_guard zoneLock(town->m_mutex);
            auto member = town->m_sessionToIndex.find(sessionID);
            if (member == town->m_sessionToIndex.end()) return false;
            auto& character = town->m_chars[member->second];
            if (character.characterID != session->second.characterID || character.hp != 0
                || character.maxHp <= 0 || character.maxMp < 0)
                return false;
            const auto [newCellX, newCellY] = GetCell(TOWN_SPAWN_X, TOWN_SPAWN_Y, town->m_area);
            auto& destinationCell = town->m_cells[newCellY][newCellX];
            if (character.cellX != newCellX || character.cellY != newCellY) {
                try {
                    destinationCell.charSessions.reserve(destinationCell.charSessions.size() + 1);
                }
                catch (const std::bad_alloc&) {
                    return false;
                }
            }
            try {
                destinationCell.dirtyChar.reserve(destinationCell.dirtyChar.size() + 1);
            }
            catch (const std::bad_alloc&) {
                return false;
            }
            const uint32_t internalID = character.zoneInternalID;
            clearPlayerEffects(*town, internalID);
            const bool movedCell = character.cellX != newCellX || character.cellY != newCellY;
            if (movedCell) town->RemoveFromCell(character);
            character.parryWindowTicks = 0;
            character.hp = character.maxHp;
            character.mp = character.maxMp;
            character.x = TOWN_SPAWN_X;
            character.y = TOWN_SPAWN_Y;
            character.dir = 0;
            character.moveBudget = 0.0f;
            character.dirtyBit |= 0x1C3; // hp, mp, direction, x and y
            if (movedCell)
                town->AddToCell(character, static_cast<uint8_t>(newCellX), static_cast<uint8_t>(newCellY));
            destinationCell.dirtyChar.push_back(internalID);
            if (movedCell) town->UpdateSessionSnapshot();
            session->second.lastRespawnRequestEpoch = request.ownershipEpoch;
            ++session->second.ownershipEpoch;
            result = {TOWN_ZONE_ID, session->second.chatID, internalID, character.x, character.y,
                town->m_generation, session->second.ownershipEpoch};
            return true;
        }

        ZoneState* first = sourceID < TOWN_ZONE_ID ? source : town;
        ZoneState* second = sourceID < TOWN_ZONE_ID ? town : source;
        std::unique_lock firstLock(first->m_mutex);
        std::unique_lock secondLock(second->m_mutex);
        auto member = source->m_sessionToIndex.find(sessionID);
        if (member == source->m_sessionToIndex.end()) return false;
        const CharacterState current = source->m_chars[member->second];
        if (current.characterID != session->second.characterID || current.hp != 0
            || current.maxHp <= 0 || current.maxMp < 0)
            return false;

        CharacterState incoming = current;
        incoming.hp = incoming.maxHp;
        incoming.mp = incoming.maxMp;
        incoming.x = TOWN_SPAWN_X;
        incoming.y = TOWN_SPAWN_Y;
        incoming.dir = 0;
        incoming.moveBudget = 0.0f;
        const auto [townCellX, townCellY] = GetCell(TOWN_SPAWN_X, TOWN_SPAWN_Y, town->m_area);
        try {
            town->m_cells[townCellY][townCellX].dirtyChar.reserve(
                town->m_cells[townCellY][townCellX].dirtyChar.size() + 1);
        }
        catch (const std::bad_alloc&) {
            return false;
        }
        if (town->ImmigrateCharLocked(sessionID, incoming) == INVALID_ZONE_INTERNAL_ID)
            return false; // The source character and session ownership are still unchanged.
        auto& entered = town->m_chars.back();
        entered.dirtyBit = 0x1C3;
        town->m_cells[entered.cellY][entered.cellX].dirtyChar.push_back(entered.zoneInternalID);
        source->RemoveCharacterLocked(sessionID);
        session->second.zoneID = TOWN_ZONE_ID;
        session->second.lastRespawnRequestEpoch = request.ownershipEpoch;
        ++session->second.ownershipEpoch;
        source->UpdateSessionSnapshot();
        town->UpdateSessionSnapshot();
        result = {TOWN_ZONE_ID, session->second.chatID, entered.zoneInternalID, entered.x, entered.y,
            town->m_generation, session->second.ownershipEpoch};
        // Joining removes the old membership; one event cannot leave a half-applied leave/join pair.
        chat->EnqueueZoneJoin(sessionID, TOWN_ZONE_ID);
        return true;
    }

    bool SessionManager::DescribeZoneChangeLocked(uint64_t sessionID, const SessionData& session, ZoneChangeResult& result) {
        auto* zone = zoneRouter->GetZone(session.zoneID);
        if (zone == nullptr)
            return false;
        std::lock_guard zoneLock(zone->m_mutex);
        const auto member = zone->m_sessionToIndex.find(sessionID);
        if (member == zone->m_sessionToIndex.end())
            return false;
        const auto& character = zone->m_chars[member->second];
        result = {session.zoneID, session.chatID, character.zoneInternalID, character.x, character.y,
            zone->m_generation, session.ownershipEpoch};
        return true;
    }

    void SessionManager::UpdateZoneOwnershipLocked(SessionData& session, uint16_t destinationID) {
        session.zoneID = destinationID;
        ++session.ownershipEpoch;
    }

    bool SessionManager::SendBootstrap(uint64_t sessionID, uint64_t ownershipEpoch) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || session->second.zoneID == 0
            || session->second.ownershipEpoch != ownershipEpoch)
            return false;
        if (session->second.bootstrapSequence == (std::numeric_limits<uint64_t>::max)())
            return false;
        const uint64_t bootstrapID = ++session->second.bootstrapSequence;
        auto zone = zoneRouter->GetZone(session->second.zoneID);
        if (zone == nullptr) return false;
        // Lock order remains shard → Zone. Capture and enqueue on the Zone's worker queue
        // happen under the Zone lock, preserving later delta/full order.
        return zone->EnqueueBootstrap(sessionID, ownershipEpoch, bootstrapID);
    }

    void SessionManager::CleanUp() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
        for (auto& shard : m_shards)
        {
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            for (auto& [session, data] : shard.sessionMap)
            {
                if (const auto& pending = data.pendingFieldTransfer) {
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
        if (it->second.zoneID == 0) {
            lobbyZone->Disconnect(sessionID);
            shard.sessionMap.erase(it);
            return;
        }
        CharacterState temp;
        const auto zoneID = it->second.zoneID;
        const auto pending = it->second.pendingFieldTransfer;
        if (pending) {
            pending->status.store(FieldZoneTransfer::Status::Complete, std::memory_order_release);
        }
        else {
            auto* zone = zoneRouter->GetZone(zoneID);
            if (zone == nullptr) {
                gameLogger->LogInfo("core session manager", "state not found", "sessionID", sessionID, "zoneID", zoneID);
                return;
            }
            if (!zone->EmigrateChar(sessionID, temp))
                return;
        }
        shard.sessionMap.erase(it);
        chat->EnqueueZoneLeave(sessionID, zoneID);
        chat->DeleteChatSession(sessionID, zoneID);
        lock.unlock();
        gameLogger->LogInfo("core session manager", "Enqueue Disconnect", "sessionID", sessionID);
        EnqueueCharacterStateUpdate(sessionID, pending ? pending->character : temp);
    }

    // rename이 DB에 확정된 뒤, 세션이 속한 zone의 CharacterState에 새 version을 반영한다.
    // zone에 이벤트 큐를 붙여 처리하는 편이 깔끔하지만, zone 간 참조 방식을 아직 정하지 못해 여기서는 ZoneState의 기존 m_mutex를 그대로 쓴다.
    void SessionManager::UpdateProfileVersion(uint64_t sessionID, uint32_t version) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end())
            return;
        uint16_t zoneID = it->second.zoneID;
        if (const auto& pending = it->second.pendingFieldTransfer) {
            pending->character.profileVersion = version;
            return;
        }

        if (zoneID == 0) // 로비에는 전파할 대상이 없다. 진입 시 full snapshot으로 최신값이 나간다.
            return;
        auto* zone = zoneRouter->GetZone(zoneID);
        if (zone == nullptr) {
            gameLogger->LogInfo("core session manager", "state not found", "sessionID", sessionID, "zoneID", zoneID);
            return;
        }
        zone->UpdateProfileVersion(sessionID, version);
    }

    void SessionManager::EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) {
        if (!cacheCommands->EnqueueCharacterStateUpdate(sessionID, character)) {
            std::vector<std::byte> binary(sizeof(CharacterState));
            std::memcpy(binary.data(), &character, sizeof(CharacterState));
            errorLogger->LogError("core session manager", "Failed to enqueue character state update", "sessionID", sessionID, "character state", binary);
        }
    }
}
