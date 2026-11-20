#include "pch.h"
#include "ZoneState.h"
#include "../Packet/PacketTypes.h"
#include "Interface/IZoneTransfer.h"
#include "WorldMap.h"
#include "../Interface/IPacket.h"
#include "../Interface/ILogger.h"
#include "SessionManager.h"
#include "../LoggerGlobal.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace Core {
    namespace {
        static_assert(TOWN_ZONE_ID == 1 && TOWN_SPAWN_X >= 0 && TOWN_SPAWN_X <= ZONE_SIZE
            && TOWN_SPAWN_Y >= 0 && TOWN_SPAWN_Y <= ZONE_SIZE,
            "Town respawn must remain inside Zone 1.");

        constexpr uint32_t RESPAWN_DIRTY_FIELDS = 0x1C3; // hp, mp, direction, x and y
    }

    void ZoneState::RemoveFromCell(CharacterState& character) {
        auto& vec = m_cells[character.cellY][character.cellX].charSessions;
        uint16_t deleteIdx = character.cellIdx;

        uint64_t swapSession = vec.back();
        vec[deleteIdx] = swapSession;

        if (deleteIdx != vec.size() - 1) {
            auto& swappedChar = m_chars[m_sessionToIndex[swapSession]];
            swappedChar.cellIdx = deleteIdx;
        }

        vec.pop_back();
    }

    void ZoneState::AddToCell(CharacterState& character, uint8_t x, uint8_t y) {
        auto& vec = m_cells[y][x].charSessions;
        vec.push_back(character.sessionID);
        character.cellIdx = vec.size() - 1;
        character.cellX = x;
        character.cellY = y;
        // 임시로 스킬 고정
    }

    uint32_t ZoneState::ImmigrateCharLocked(uint64_t sessionID, const CharacterState& state) {
        if (m_chars.size() >= MAX_ZONE_CAPACITY || m_sessionToIndex.contains(sessionID)
            || m_generation == (std::numeric_limits<uint64_t>::max)()
            || !std::isfinite(state.x) || !std::isfinite(state.y)
            || state.x < m_area.x_min || state.x > m_area.x_max
            || state.y < m_area.y_min || state.y > m_area.y_max
            || !WorldMap::CanTraverse(state.x, state.y, state.x, state.y))
            return INVALID_ZONE_INTERNAL_ID;
        const uint32_t id = m_internalIdGenerator.load(std::memory_order_relaxed);
        if (id == INVALID_ZONE_INTERNAL_ID || id >= MAX_ZONE_INTERNAL_ID
            || m_InternalIDToIndex.contains(id))
            return INVALID_ZONE_INTERNAL_ID;
        try {
            CharacterState incoming = state;
            incoming.sessionID = sessionID;
            incoming.zoneInternalID = id;
            incoming.lastZone = m_zoneID;
            incoming.dirtyBit = 0;
            incoming.parryWindowTicks = 0;
            if (incoming.skillSlot.empty()) {
                incoming.skillSlot = {{0, 0}, {1, 0}, {2, 0}};
            }
            else if (incoming.skillSlot.size() != 3) {
                if (incoming.skillSlot.size() < 3)
                    return INVALID_ZONE_INTERNAL_ID;
                incoming.skillSlot.resize(3);
            }
            incoming.skillSlotCnt = 3;
            if (!RegisterCharacterLocked(std::move(incoming)))
                return INVALID_ZONE_INTERNAL_ID;
        }
        catch (const std::bad_alloc&) {
            return INVALID_ZONE_INTERNAL_ID;
        }
        m_internalIdGenerator.store(id + 1, std::memory_order_relaxed);
        ++m_generation;
        return id;
    }

    bool ZoneState::AdmitLobbyCharacter(uint64_t sessionID, CharacterState incoming, ZoneChangeResult& result) {
        std::lock_guard lock(m_mutex);
        if (!std::isfinite(incoming.x) || !std::isfinite(incoming.y))
            return false; // Corrupt coordinates are not a map-version relocation.
        if (incoming.x < m_area.x_min || incoming.x > m_area.x_max
            || incoming.y < m_area.y_min || incoming.y > m_area.y_max
            || !WorldMap::CanTraverse(incoming.x, incoming.y, incoming.x, incoming.y)) {
            // Saved coordinates can become invalid after a fixed-map revision. Relocate only
            // on lobby entry; live Zone transfers must still prove their existing position.
            incoming.x = m_area.x_min + TRANSITION_BUFFER + 10.0f;
            incoming.y = m_area.y_min + TRANSITION_BUFFER + 10.0f;
            if (!WorldMap::CanTraverse(incoming.x, incoming.y, incoming.x, incoming.y)) {
                const auto& safeArea = m_cells[0][0].area;
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
        if (ImmigrateCharLocked(sessionID, incoming) == INVALID_ZONE_INTERNAL_ID)
            return false;
        const auto& entered = m_chars.back();
        result = {m_zoneID, 0, entered.zoneInternalID, entered.x, entered.y, m_generation};
        return true;
    }

    bool ZoneState::DescribeOrRespawnInTown(uint64_t sessionID, uint64_t characterID, bool replay,
        ZoneChangeResult& result) {
        if (!WorldMap::CanTraverse(TOWN_SPAWN_X, TOWN_SPAWN_Y, TOWN_SPAWN_X, TOWN_SPAWN_Y))
            return false;
        std::lock_guard lock(m_mutex);
        const auto member = m_sessionToIndex.find(sessionID);
        if (member == m_sessionToIndex.end())
            return false;

        auto& character = m_chars[member->second];
        if (character.characterID != characterID)
            return false;
        if (replay) {
            if (character.hp <= 0 || character.hp != character.maxHp || character.mp != character.maxMp
                || character.x != TOWN_SPAWN_X || character.y != TOWN_SPAWN_Y)
                return false;
        } else {
            if (character.hp != 0 || character.maxHp <= 0 || character.maxMp < 0)
                return false;

            const auto [x, y] = GetCell(TOWN_SPAWN_X, TOWN_SPAWN_Y, m_area);
            auto& destination = m_cells[y][x];
            const bool movedCell = character.cellX != x || character.cellY != y;
            try {
                if (movedCell)
                    destination.charSessions.reserve(destination.charSessions.size() + 1);
                destination.dirtyChar.reserve(destination.dirtyChar.size() + 1);
            }
            catch (const std::bad_alloc&) {
                return false;
            }
            ClearCharacterEffects(sessionID, character.zoneInternalID);
            if (movedCell)
                RemoveFromCell(character);
            character.parryWindowTicks = 0;
            character.hp = character.maxHp;
            character.mp = character.maxMp;
            character.x = TOWN_SPAWN_X;
            character.y = TOWN_SPAWN_Y;
            character.dir = 0;
            character.moveBudget = 0.0f;
            character.dirtyBit |= RESPAWN_DIRTY_FIELDS;
            if (movedCell)
                AddToCell(character, x, y);
            destination.dirtyChar.push_back(character.zoneInternalID);
        }
        result = {m_zoneID, 0, character.zoneInternalID, character.x, character.y, m_generation};
        return true;
    }

    bool ZoneState::StartRespawnTransfer(uint64_t sessionID,
        uint64_t characterID, uint64_t ownershipEpoch, IZoneTransfer& zoneTransfer) {
        if (!WorldMap::CanTraverse(TOWN_SPAWN_X, TOWN_SPAWN_Y, TOWN_SPAWN_X, TOWN_SPAWN_Y))
            return false;
        std::shared_ptr<ZoneTransfer> transfer;
        {
            std::lock_guard zoneLock(m_mutex);
            const auto member = m_sessionToIndex.find(sessionID);
            if (member == m_sessionToIndex.end())
                return false;
            const auto& character = m_chars[member->second];
            if (character.characterID != characterID || character.hp != 0
                || character.maxHp <= 0 || character.maxMp < 0)
                return false;

            transfer = std::make_shared<ZoneTransfer>();
            transfer->character = character;
            transfer->ownershipEpoch = ownershipEpoch;
            transfer->sourceZoneID = m_zoneID;
            transfer->kind = ZoneTransfer::Kind::Respawn;
            transfer->x = TOWN_SPAWN_X;
            transfer->y = TOWN_SPAWN_Y;
        }
        return TryStartZoneTransfer(TOWN_ZONE_ID, transfer, zoneTransfer);
    }

    bool ZoneState::TryStartZoneTransfer(uint16_t destination, const std::shared_ptr<ZoneTransfer>& transfer,
        IZoneTransfer& zoneTransfer) {
        // Register before publishing; never take the session lock under the Zone lock.
        if (!sessionManager->BeginZoneTransfer(transfer))
            return false;
        const auto sessionID = transfer->character.sessionID;
        {
            std::lock_guard zoneLock(m_mutex);
            if (zoneTransfer.TryEnqueueTransfer(destination, transfer)) {
                RemoveCharacterLocked(sessionID);
                return true;
            }
        }
        sessionManager->CancelZoneTransfer(transfer);
        // Preserve a rename that reached the pending record before enqueue failed.
        UpdateProfileVersion(sessionID, transfer->character.profileId, transfer->character.profileVersion);
        return false;
    }

    bool ZoneState::TryResolveZoneTransfer(const ZoneTransfer& transfer, bool returning, ZoneChangeResult& result) {
        std::lock_guard lock(m_mutex);
        try {
            if (returning)
                return RestoreZoneTransferLocked(transfer.character);
            if (!AdmitZoneTransferLocked(transfer))
                return false;
        }
        catch (const std::bad_alloc&) {
            return false;
        }

        const auto& entered = m_chars.back();
        result = {m_zoneID, 0, entered.zoneInternalID, entered.x, entered.y, m_generation};
        return true;
    }

    bool ZoneState::AdmitZoneTransferLocked(const ZoneTransfer& transfer) {
        CharacterState incoming = transfer.character;
        incoming.x = transfer.x;
        incoming.y = transfer.y;
        incoming.dir = transfer.dir;
        incoming.moveBudget = transfer.moveBudget;
        if (!transfer.IsRespawn())
            return ImmigrateCharLocked(incoming.sessionID, incoming) != INVALID_ZONE_INTERNAL_ID;

        incoming.hp = incoming.maxHp;
        incoming.mp = incoming.maxMp;
        const auto [x, y] = GetCell(incoming.x, incoming.y, m_area);
        auto& dirty = m_cells[y][x].dirtyChar;
        dirty.reserve(dirty.size() + 1);
        if (ImmigrateCharLocked(incoming.sessionID, incoming) == INVALID_ZONE_INTERNAL_ID)
            return false;
        auto& entered = m_chars.back();
        entered.dirtyBit = RESPAWN_DIRTY_FIELDS;
        dirty.push_back(entered.zoneInternalID);
        return true;
    }

    bool ZoneState::RestoreZoneTransferLocked(const CharacterState& original) {
        CharacterState incoming = original;
        return RegisterCharacterLocked(std::move(incoming));
    }

    bool ZoneState::RegisterCharacterLocked(CharacterState&& incoming) {
        static_assert(std::is_nothrow_move_constructible_v<CharacterState>);
        if (m_chars.size() >= MAX_ZONE_CAPACITY)
            return false;

        const auto sessionID = incoming.sessionID;
        const auto id = incoming.zoneInternalID;
        const auto index = static_cast<uint16_t>(m_chars.size());
        const auto [x, y] = GetCell(incoming.x, incoming.y, m_area);
        auto& cellSessions = m_cells[y][x].charSessions;
        cellSessions.reserve(cellSessions.size() + 1);
        const auto [sessionEntry, inserted] = m_sessionToIndex.emplace(sessionID, index);
        if (!inserted)
            return false;

        struct SessionIndexRollback {
            decltype(m_sessionToIndex)& sessionIndex;
            decltype(m_sessionToIndex)::iterator entry;
            bool committed = false;

            ~SessionIndexRollback() {
                if (!committed)
                    sessionIndex.erase(entry);
            }
        } rollback{m_sessionToIndex, sessionEntry};

        if (!m_InternalIDToIndex.emplace(id, index).second)
            return false;
        m_chars.push_back(std::move(incoming));
        AddToCell(m_chars.back(), x, y);
        rollback.committed = true;
        return true;
    }

    void ZoneState::ClearCharacterEffects(uint64_t sessionID, uint32_t id) {
        // Old casts must not find the same session again after a quick return.
        for (auto& row : m_cells) {
            for (auto& cell : row) {
                std::erase_if(cell.activeSkills, [sessionID](const ActiveSkill& skill) {
                    return skill.casterType == 0 && skill.sessionId == sessionID;
                });
                std::erase_if(cell.actionResults, [id](const ActionResult& result) {
                    return result.casterType == 0 && result.zoneInternalId == id;
                });
                std::erase(cell.dirtyChar, id);
            }
        }
    }

    void ZoneState::RemoveCharacterLocked(uint64_t sessionID) {
        auto it = m_sessionToIndex.find(sessionID);
        auto& character = m_chars[it->second];
        const uint32_t id = character.zoneInternalID;
        RemoveFromCell(character);
        ClearCharacterEffects(sessionID, id);
        const auto index = it->second;
        if (index != m_chars.size() - 1) {
            m_chars[index] = std::move(m_chars.back());
            m_sessionToIndex.at(m_chars[index].sessionID) = index;
            m_InternalIDToIndex.at(m_chars[index].zoneInternalID) = index;
        }
        m_chars.pop_back();
        m_sessionToIndex.erase(it);
        m_InternalIDToIndex.erase(id);
    }

    void ZoneState::RestoreMovementLocked(uint64_t sessionID, float x, float y, uint8_t dir, float moveBudget, uint32_t dirtyBit) {
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) return;
        auto& character = m_chars[it->second];
        if (character.x != x || character.y != y) {
            RemoveFromCell(character);
            character.x = x;
            character.y = y;
            auto [cellX, cellY] = GetCell(x, y, m_area);
            AddToCell(character, static_cast<uint8_t>(cellX), static_cast<uint8_t>(cellY));
        }
        character.dir = dir;
        character.moveBudget = moveBudget;
        character.dirtyBit = dirtyBit;
    }

    bool ZoneState::EmigrateChar(uint64_t sessionID, CharacterState& out) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end())
            return false;
        out = m_chars[it->second];
        RemoveCharacterLocked(sessionID);
        return true;
    }

    void ZoneState::UpdateProfileVersion(uint64_t sessionID, uint32_t profileID, uint32_t version) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end())
            return;
        auto& character = m_chars[it->second];
        if (!character.UpdateProfileVersion(profileID, version))
            return;
        character.dirtyBit |= 0x200;
        m_cells[character.cellY][character.cellX].dirtyChar.push_back(character.zoneInternalID);
    }

    void ZoneState::FlushCheat() {
        std::vector<std::pair<uint64_t, uint16_t>> cheats;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cheats.swap(m_cheatList);
        }
        const auto timePoint = std::chrono::steady_clock::now();
        for (auto [session, cheat] : cheats)
            sessionManager->Cheat(session, cheat, timePoint);
    }

    bool ZoneState::TakeCharacter(CharacterState& out) {
        std::lock_guard lock(m_mutex);
        if (m_chars.empty()) return false;
        out = m_chars.back();
        RemoveCharacterLocked(out.sessionID);
        return true;
    }
}
