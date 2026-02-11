#include "pch.h"
#include "ZoneState.h"
#include "IPacket.h"
#include "ILogger.h"
#include "PacketWriter.h"
#include "StateManager.h"

#include <iostream>

namespace Core {
    void ZoneState::RemoveFromCell(CharacterState& character) {
        std::cout << "remove Cell: " << character.cellIdx << " x " << (int)character.cellX << " y " << (int)character.cellY << "\n";
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
        character.skillSlotCnt = 3;
        character.skillSlot.push_back(SkillSlotEntry(0, 0));
        character.skillSlot.push_back(SkillSlotEntry(1, 0));
        character.skillSlot.push_back(SkillSlotEntry(2, 0));
        std::cout << "add Cell: " << character.cellIdx <<  " x " << (int)x << " y " << (int)y << "\n";
    }

    uint64_t ZoneState::ImmigrateChar(uint64_t sessionID, CharacterState& state) {
        // 추가 제거는 fullSnapshot에 적용됨
        state.zoneInternalID = m_internalIdGenerator.fetch_add(1);
        std::lock_guard<std::mutex> lock(m_mutex);
        
        uint16_t index = static_cast<uint16_t>(m_chars.size());
        if(index >= MAX_ZONE_CAPACITY)
            return 0;
        auto it = m_sessionToIndex.find(sessionID);
        if (it != m_sessionToIndex.end()) {
            logger->LogError(std::format("ImmigrageChar Failed sessionID already exists in zone {}! sessionID={}", m_zoneID, sessionID));
            return 0;
        }
        auto [x, y] = GetCell(state.x, state.y, m_area);
        AddToCell(state, x, y);
        state.lastZone = m_zoneID;
        m_chars.push_back(state);
        m_sessionToIndex[sessionID] = index;
        m_InternalIDToIndex[state.zoneInternalID] = index;
        std::cout << "immigrate zone: " << m_zoneID << " x " << x << " y " << y << "\n";
        stateManager->SetZoneID(sessionID, m_zoneID);
        
        return state.zoneInternalID;
    }
    
    bool ZoneState::EmigrateChar(uint64_t sessionID, CharacterState& o) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()){
            logger->LogError(std::format("EmigrageChar Failed sessionID not exists in zone {}! sessionID={}",m_zoneID, sessionID));
            return false;
        }
        o = m_chars[it->second]; // out 복사
        RemoveFromCell(m_chars[it->second]);
        m_chars[it->second] = m_chars.back(); // swap

        m_sessionToIndex[m_chars[it->second].sessionID] = it->second;
        m_InternalIDToIndex[m_chars[it->second].zoneInternalID] = it->second;

        m_chars.pop_back();
        m_sessionToIndex.erase(sessionID); 
        m_InternalIDToIndex.erase(o.zoneInternalID); 
        logger->LogInfo("After EmigrateChar, chars.size=" + std::to_string(m_chars.size()));
        return true;
    }

    void ZoneState::FlushCheat() {
        std::lock_guard<std::mutex> lock(m_mutex);
        // now()가 무거워서 근사값으로 처리
        auto timePoint = std::chrono::steady_clock::now();
        for (auto [session, cheat]: m_cheatList)
        {
            stateManager->Cheat(session, cheat, timePoint);
        }
    }
}
