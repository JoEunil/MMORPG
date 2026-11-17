#include "pch.h"
#include "ZoneState.h"
#include "WorldMap.h"
#include "IPacket.h"
#include "ILogger.h"
#include "SessionManager.h"
#include "LoggerGlobal.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace Core {
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

    bool ZoneState::AdmitFieldTransferLocked(const FieldZoneTransfer& transfer) {
        CharacterState incoming = transfer.character;
        incoming.x = transfer.x;
        incoming.y = transfer.y;
        incoming.dir = transfer.dir;
        incoming.moveBudget = transfer.moveBudget;
        return ImmigrateCharLocked(incoming.sessionID, incoming) != INVALID_ZONE_INTERNAL_ID;
    }

    bool ZoneState::RestoreFieldTransferLocked(const CharacterState& original) {
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

    void ZoneState::RemoveCharacterLocked(uint64_t sessionID) {
        auto it = m_sessionToIndex.find(sessionID);
        auto& character = m_chars[it->second];
        const uint32_t id = character.zoneInternalID;
        RemoveFromCell(character);
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
        if (character.profileId != profileID || character.profileVersion >= version)
            return;
        character.profileVersion = version;
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
}
