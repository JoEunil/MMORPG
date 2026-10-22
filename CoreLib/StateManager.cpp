#include "pch.h"
#include "StateManager.h"
#include "Initializer.h"
#include "ChatThreadPool.h"
#include "WorldMap.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Core {
    static_assert(WorldMap::FormatVersion == 1 && WorldMap::Zones.size() == ZONE_COUNT
        && WorldMap::ZoneColumns == ZONE_HORIZON && WorldMap::ZoneRows == ZONE_VERTICAL,
        "Exported zone layout must match the configured server world.");
    static_assert(WorldMap::ZoneCoreSize == ZONE_SIZE && WorldMap::TransitionMargin == TRANSITION_BUFFER
        && CELLS_X == 5 && CELLS_Y == 5 && WorldMap::Zones[0].aoiCellSize == CELL_SIZE,
        "Exported zone bounds and 5x5 AOI cells must match the server spatial configuration.");
    static_assert(TOWN_ZONE_ID == 1 && TOWN_SPAWN_X >= 0 && TOWN_SPAWN_X <= ZONE_SIZE
        && TOWN_SPAWN_Y >= 0 && TOWN_SPAWN_Y <= ZONE_SIZE,
        "Town respawn must remain inside Zone 1.");

    static_assert(WorldMap::FormatVersion == 1);
    static_assert(WorldMap::ZoneColumns == ZONE_HORIZON && WorldMap::ZoneRows == ZONE_VERTICAL);
    static_assert(WorldMap::Zones.size() == ZONE_COUNT);
    static_assert(WorldMap::ZoneCoreSize == ZONE_SIZE && WorldMap::TransitionMargin == TRANSITION_BUFFER);

    bool StateManager::ChangeZone(uint64_t sessionID, uint8_t op, ZoneChangeResult& result) {
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
            auto destination = GetZone(destinationID);
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

    bool StateManager::RespawnToTown(uint64_t sessionID, const RespawnToTownBody& request, ZoneChangeResult& result) {
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
        auto source = GetZone(sourceID);
        auto town = GetZone(TOWN_ZONE_ID);
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

    bool StateManager::ProcessAction(uint64_t sessionID, uint16_t routeZoneID, const ActionRequestBody& action, ZoneChangeResult& transition) {
        transition = {};
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated || !session->second.authCompleted
            || session->second.zoneID == 0 || session->second.zoneID > ZONE_COUNT)
            return false;
        if (session->second.zoneID != routeZoneID || session->second.ownershipEpoch != action.ownershipEpoch) {
            auto currentZone = GetZone(session->second.zoneID);
            if (currentZone == nullptr || session->second.ownershipEpoch == 0) return false;
            std::lock_guard zoneLock(currentZone->m_mutex);
            auto current = currentZone->m_sessionToIndex.find(sessionID);
            if (current == currentZone->m_sessionToIndex.end()) return false;
            const auto& character = currentZone->m_chars[current->second];
            transition = {session->second.zoneID, session->second.chatID, character.zoneInternalID, character.x, character.y,
                currentZone->m_generation, session->second.ownershipEpoch};
            return true;
        }
        session->second.lastRespawnRequestEpoch = 0;
        auto zone = GetZone(routeZoneID);
        if (zone == nullptr)
            return false;
        float previousX = 0, previousY = 0, previousBudget = 0;
        uint8_t previousDir = 0;
        uint16_t previousDirty = 0;
        {
            std::lock_guard sourceLock(zone->m_mutex);
            auto it = zone->m_sessionToIndex.find(sessionID);
            if (it == zone->m_sessionToIndex.end()) return false;
            const auto& current = zone->m_chars[it->second];
            previousX = current.x; previousY = current.y; previousBudget = current.moveBudget;
            previousDir = current.dir; previousDirty = current.dirtyBit;
        }
        zone->Move(sessionID, action.dir, action.x, action.y);
        float x = 0, y = 0;
        {
            std::lock_guard sourceLock(zone->m_mutex);
            auto it = zone->m_sessionToIndex.find(sessionID);
            if (it == zone->m_sessionToIndex.end()) return false;
            const auto& current = zone->m_chars[it->second];
            x = current.x; y = current.y;
        }
        const int column = (routeZoneID - 1) % ZONE_HORIZON;
        const int row = (routeZoneID - 1) / ZONE_HORIZON;
        const float coreMinX = column * ZONE_SIZE;
        const float coreMinY = row * ZONE_SIZE;
        const int nextColumnRaw = x >= coreMinX + ZONE_SIZE + TRANSITION_BUFFER ? column + 1
            : x <= coreMinX - TRANSITION_BUFFER ? column - 1 : column;
        const int nextRowRaw = y >= coreMinY + ZONE_SIZE + TRANSITION_BUFFER ? row + 1
            : y <= coreMinY - TRANSITION_BUFFER ? row - 1 : row;
        // Do not let an out-of-layout column wrap into another valid row's zone ID.
        const int nextColumn = (std::clamp)(nextColumnRaw, 0, static_cast<int>(ZONE_HORIZON) - 1);
        const int nextRow = (std::clamp)(nextRowRaw, 0, static_cast<int>(ZONE_VERTICAL) - 1);
        const int destinationID = nextRow * ZONE_HORIZON + nextColumn + 1;
        bool mayTransition = true;
        if (destinationID >= 1 && destinationID <= ZONE_COUNT && destinationID != routeZoneID
            && session->second.ownershipEpoch != (std::numeric_limits<uint64_t>::max)()) {
            if (nextColumn != column && nextRow != row) {
                const auto& layout = WorldMap::Zones[routeZoneID - 1];
                bool directCrossingAllowed = false;
                for (size_t i = 0; i < layout.neighborIds.size(); ++i) {
                    if (layout.neighborIds[i] == destinationID) {
                        directCrossingAllowed = layout.directCrossings[i];
                        break;
                    }
                }
                mayTransition = directCrossingAllowed;
            }
        }
        if (mayTransition && destinationID >= 1 && destinationID <= ZONE_COUNT && destinationID != routeZoneID
            && session->second.ownershipEpoch != (std::numeric_limits<uint64_t>::max)()) {
            auto destination = GetZone(static_cast<uint16_t>(destinationID));
            if (destination != nullptr) {
                std::unique_lock firstLock(routeZoneID < destinationID ? zone->m_mutex : destination->m_mutex);
                std::unique_lock secondLock(routeZoneID < destinationID ? destination->m_mutex : zone->m_mutex);
                auto it = zone->m_sessionToIndex.find(sessionID);
                if (it == zone->m_sessionToIndex.end() || session->second.zoneID != routeZoneID
                    || session->second.ownershipEpoch != action.ownershipEpoch)
                    return false;
                CharacterState current = zone->m_chars[it->second];
                if (destination->ImmigrateCharLocked(sessionID, current) == INVALID_ZONE_INTERNAL_ID) {
                    zone->RestoreMovementLocked(sessionID, previousX, previousY, previousDir, previousBudget, previousDirty);
                    return false;
                }
                zone->RemoveCharacterLocked(sessionID);
                session->second.zoneID = static_cast<uint16_t>(destinationID);
                ++session->second.ownershipEpoch;
                zone->UpdateSessionSnapshot();
                destination->UpdateSessionSnapshot();
                const auto& entered = destination->m_chars.back();
                transition = {static_cast<uint16_t>(destinationID), session->second.chatID, entered.zoneInternalID,
                    entered.x, entered.y, destination->m_generation, session->second.ownershipEpoch};
                chat->EnqueueZoneJoin(sessionID, static_cast<uint16_t>(destinationID));
                secondLock.unlock();
                firstLock.unlock();
                destination->Skill(sessionID, action.skillSlot);
                destination->DirtyCheck(sessionID);
                return true;
            }
        }
        zone->Skill(sessionID, action.skillSlot);
        zone->DirtyCheck(sessionID);
        return true;
    }

    bool StateManager::SendBootstrap(uint64_t sessionID, uint64_t ownershipEpoch) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || session->second.zoneID == 0
            || session->second.ownershipEpoch != ownershipEpoch)
            return false;
        if (session->second.bootstrapSequence == (std::numeric_limits<uint64_t>::max)())
            return false;
        const uint64_t bootstrapID = ++session->second.bootstrapSequence;
        auto zone = GetZone(session->second.zoneID);
        if (zone == nullptr) return false;
        // Lock order remains shard → Zone. Snapshot construction and registration on the single
        // broadcast queue happen while the Zone lock is held, preserving later delta/full order.
        return zone->EnqueueBootstrap(sessionID, ownershipEpoch, bootstrapID);
    }

    void StateManager::CleanUp() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
        for (auto& shard : m_shards)
        {
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            for (auto& [session, data] : shard.sessionMap)
            {
                CharacterState temp;
                if (data.zoneID == 0) {
                    if (lobbyZone->EmigrateChar(session, temp) == false) {
                        continue;
                    }
                }
                else {
                    auto it_state = m_states.find(data.zoneID);
                    if (it_state == m_states.end()) {
                        gameLogger->LogInfo("state manager", "state not found", "sessionID", session, "zoneID", data.zoneID);
                        continue;
                    }
                    if (it_state->second->EmigrateChar(session, temp) == false) {
                        continue;
                    }
                }
                Message msg(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
                auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg.GetBuffer());

                st->header.sessionID = session;
                st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
                st->body.charID = temp.characterID;
                st->body.profileId = temp.profileId;
                st->body.attack = temp.attack;
                st->body.level = temp.level;
                st->body.exp = temp.exp;
                st->body.hp = temp.hp;
                st->body.mp = temp.mp;
                st->body.maxHp = temp.maxHp;
                st->body.maxMp = temp.maxMp;
                st->body.dir = temp.dir;
                st->body.x = temp.x;
                st->body.y = temp.y;
                st->body.lastZone = static_cast<uint8_t>(temp.lastZone);
				msg.SetLength(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
                if (!mq->EnqueueMessage(&msg)) {
                    std::vector<std::byte> binary(sizeof(CharacterState));
                    std::memcpy(binary.data(), &temp, sizeof(CharacterState));
                    Core::errorLogger->LogError("state manager", "failed to enqueue message for disconnect", "sessionID", session, "character state", binary);
                }
            }
            shard.sessionMap.clear();
        }
    }

    void StateManager::Disconnect(uint64_t sessionID) {
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
        auto it_state = m_states.find(it->second.zoneID);
        if (it_state == m_states.end()) {
            gameLogger->LogInfo("state manager", "state not found", "sessionID", sessionID, "zoneID", it->second.zoneID);
            return;
        }
        if (it_state->second->EmigrateChar(sessionID, temp)) {
            auto zoneID = it->second.zoneID;
            shard.sessionMap.erase(it);
            chat->EnqueueZoneLeave(sessionID, zoneID);
            chat->DeleteChatSession(sessionID, zoneID);
            lock.unlock();
            EnqueueDisconnectMsg(temp, sessionID);
        }
    }

    // rename이 DB에 확정된 뒤, 세션이 속한 zone의 CharacterState에 새 version을 반영한다.
    // zone에 이벤트 큐를 붙여 처리하는 편이 깔끔하지만, zone 간 참조 방식을 아직 정하지 못해 여기서는 ZoneState의 기존 m_mutex를 그대로 쓴다.
    void StateManager::UpdateProfileVersion(uint64_t sessionID, uint32_t version) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock<std::shared_mutex> lock(shard.smutex);
        auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end())
            return;
        uint16_t zoneID = it->second.zoneID;
        lock.unlock();

        if (zoneID == 0) // 로비에는 전파할 대상이 없다. 진입 시 full snapshot으로 최신값이 나간다.
            return;
        auto it_state = m_states.find(zoneID);
        if (it_state == m_states.end()) {
            gameLogger->LogInfo("state manager", "state not found", "sessionID", sessionID, "zoneID", zoneID);
            return;
        }
        it_state->second->UpdateProfileVersion(sessionID, version);
    }

    void StateManager::EnqueueDisconnectMsg(CharacterState& temp, uint64_t sessionID)
    {
        gameLogger->LogInfo("state manager", "Enqueue Disconnect", "sessionID", sessionID);
        Message msg(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
        auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg.GetBuffer());
        st->header.sessionID = sessionID;
        st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
        st->body.charID = temp.characterID;
        st->body.profileId = temp.profileId;
        st->body.attack = temp.attack;
        st->body.level = temp.level;
        st->body.exp = temp.exp;
        st->body.hp = temp.hp;
        st->body.mp = temp.mp;
        st->body.maxHp = temp.maxHp;
        st->body.maxMp = temp.maxMp;
        st->body.dir = temp.dir;
        st->body.x = temp.x;
        st->body.y = temp.y;
        st->body.lastZone = static_cast<uint8_t>(temp.lastZone);
        msg.SetLength(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));

        if (!mq->EnqueueMessage(&msg)) {
            std::vector<std::byte> binary(sizeof(CharacterState));
            std::memcpy(binary.data(), &temp, sizeof(CharacterState));
            errorLogger->LogError("state manager", "Failed to enqueue disconnect message", "sessionID", sessionID, "character state", binary);
        }
    }
}
