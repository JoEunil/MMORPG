#include "pch.h"
#include "ZoneState.h"
#include "IPacket.h"
#include "Initializer.h"
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

    void ZoneState::UpdateSessionSnapshot() {
        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                int idx = i * CELLS_X + j;
                (*sessionSnapshotWriter)[idx].clear();
                for (auto& session : m_cells[i][j].charSessions)
                    (*sessionSnapshotWriter)[idx].push_back(session);
            }
        }
        tripleBuffer.Write(sessionSnapshotWriter);
    }

    void ZoneState::DeltaSnapshot() {
        std::lock_guard<std::mutex> lock(m_mutex);
        UpdateSessionSnapshot();

        std::vector<std::shared_ptr<IPacket>> packets;
        packets.resize(CELLS_X * CELLS_Y);

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                int idx = i * CELLS_X + j;
                packets[idx] = writer->GetInitialDeltaPacket();
            }
        }

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                auto& cell = m_cells[i][j];
                int idx = i * CELLS_X + j;
                int loop = DELTA_UPDATE_COUNT;

                bool wroteField = false; 
                while (!cell.dirtyChar.empty() && loop--)
                {
                    auto& internalID = cell.dirtyChar.back();
                    auto it = m_InternalIDToIndex.find(internalID);
                    if (it != m_InternalIDToIndex.end()) {
                        auto& character = m_chars[it->second];
                        auto& bit = character.dirtyBit;
                        if (bit & 0x01)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 0, character.hp);
                        if (bit & 0x02)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 1, character.mp);
                        if (bit & 0x04)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 2, character.maxHp);
                        if (bit & 0x08)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 3, character.maxMp);
                        if (bit & 0x10)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 4, character.exp);
                        if (bit & 0x20)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 5, character.level);
                        if (bit & 0x40)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 6, character.dir);
                        if (bit & 0x80)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 7, character.x);
                        if (bit & 0x100)
                            writer->WriteDeltaField(packets[idx], character.zoneInternalID, 8, character.y);
                        character.dirtyBit = 0x00;
                    }
                    cell.dirtyChar.pop_back();
                    wroteField = true;
                }

                while (loop-- && cell.dirtyMonster.size() > 0)
                {

                }

                if (!wroteField) {
                    packets[idx].reset();
                    packets[idx] = nullptr;
                }
            }


        }
        
        broadcast->EnqueueWork(packets, m_zoneID);
    }

    void ZoneState::FullSnapshot() {
        std::lock_guard<std::mutex> lock(m_mutex);
        UpdateSessionSnapshot();
        if (m_chars.empty())
            return;
        std::vector<std::shared_ptr<IPacket>> packets;
        packets.resize(CELLS_X * CELLS_Y);

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                int idx = i * CELLS_X + j;
                packets[idx] = writer->GetInitialFullPacket();
            }
        }

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                auto& cell = m_cells[i][j];
                cell.dirtyChar.clear();
                cell.dirtyMonster.clear();
                int idx = i * CELLS_X + j;
                for (auto& session : cell.charSessions)
                {
                    auto& character = m_chars[m_sessionToIndex[session]];
                    writer->WriteFullField(packets[idx], character);
                }
                for (auto& mon : cell.monsterIndexes)
                {

                }
            }
        }
        broadcast->EnqueueWork(packets, m_zoneID);
    }

    bool ZoneState::Move(uint64_t sessionID, uint8_t dir, float speed) {
        float x = 0;
        float y = 0;
        if (speed > 1 || speed < 0)
            return false;
        switch(dir) {
            case 0: y = speed; break;
            case 1: y = -speed; break;
            case 2: x = -speed; break;
            case 3: x = speed; break;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return false;
        }
        auto& character = m_chars[it->second];
        // move 영역 제한
        if (character.x + x > m_area.x_max) x = 0;
        if (character.x + x < m_area.x_min) x = 0;
        if (character.y + y > m_area.y_max) y = 0;
        if (character.y + y < m_area.y_min) y = 0;
        
        character.x += x;
        character.y += y;
        character.dir = dir;
        character.dirtyBit |= 0x40; // dir
        if (x)
            character.dirtyBit |= 0x80; // x
        if (y)
            character.dirtyBit |= 0x100; // y


        auto [newX, newY] = GetCell(character.x, character.y, m_area);

        uint16_t oldX = character.cellX;
        uint16_t oldY = character.cellY;
        uint16_t oldIdx = character.cellIdx;
        // cell 변경 발생할 경우만
        if (newX != oldX || newY != oldY)
        {
            RemoveFromCell(character);
            AddToCell(character, newX, newY);
        }
        return true;
    }

    void ZoneState::DirtyCheck(uint64_t sessionID) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }
        auto& character = m_chars[it->second];
        m_cells[character.cellY][character.cellX].dirtyChar.push_back(character.zoneInternalID);
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


    void ZoneState::UpdateMonster() {

    }
}
