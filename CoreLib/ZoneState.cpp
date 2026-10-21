#include "pch.h"
#include "ZoneState.h"
#include "WorldMap.h"
#include "IPacket.h"
#include "ILogger.h"
#include "StateManager.h"
#include "LoggerGlobal.h"
#include <algorithm>
#include <cmath>
#include <limits>

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

    // The caller owns m_mutex (and the session shard for production transfers).
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
        const auto index = static_cast<uint16_t>(m_chars.size());
        const auto [x, y] = GetCell(state.x, state.y, m_area);
        bool sessionInserted = false;
        bool internalInserted = false;
        try {
            // Complete every allocating operation before changing source membership or ID/generation.
            CharacterState incoming = state;
            incoming.sessionID = sessionID;
            incoming.zoneInternalID = id;
            incoming.lastZone = m_zoneID;
            incoming.dirtyBit = 0;
            if (incoming.skillSlot.empty()) {
                incoming.skillSlot = {{0, 0}, {1, 0}, {2, 0}};
            }
            else if (incoming.skillSlot.size() != 3) {
                if (incoming.skillSlot.size() < 3)
                    return INVALID_ZONE_INTERNAL_ID;
                incoming.skillSlot.resize(3);
            }
            incoming.skillSlotCnt = 3;
            auto& sessions = m_cells[y][x].charSessions;
            sessions.reserve(sessions.size() + 1);
            sessionInserted = m_sessionToIndex.emplace(sessionID, index).second;
            if (!sessionInserted)
                return INVALID_ZONE_INTERNAL_ID;
            internalInserted = m_InternalIDToIndex.emplace(id, index).second;
            if (!internalInserted) {
                m_sessionToIndex.erase(sessionID);
                return INVALID_ZONE_INTERNAL_ID;
            }
            // m_chars capacity is reserved at construction; moving CharacterState does not allocate.
            m_chars.push_back(std::move(incoming));
            AddToCell(m_chars.back(), x, y);
        }
        catch (const std::bad_alloc&) {
            if (internalInserted) m_InternalIDToIndex.erase(id);
            if (sessionInserted) m_sessionToIndex.erase(sessionID);
            return INVALID_ZONE_INTERNAL_ID;
        }
        m_internalIdGenerator.store(id + 1, std::memory_order_relaxed);
        ++m_generation;
        return id;
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

    bool ZoneState::EmigrateChar(uint64_t sessionID, CharacterState& out) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end())
            return false;
        out = m_chars[it->second];
        RemoveCharacterLocked(sessionID);
        return true;
    }

    // rename 반영. 
    void ZoneState::UpdateProfileVersion(uint64_t sessionID, uint32_t version) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end())
            return;
        auto& character = m_chars[it->second];
        if (character.profileVersion == version)
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
            stateManager->Cheat(session, cheat, timePoint);
    }
}
