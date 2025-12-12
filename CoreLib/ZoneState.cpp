#include "pch.h"
#include "ZoneState.h"
#include "IPacket.h"
#include "Initializer.h"
#include "ILogger.h"
#include "PacketWriter.h"
#include "StateManager.h"

#include <iostream>

namespace Core {
    uint64_t ZoneState::ImmigrateChar(uint64_t sessionID, CharacterState& state) {
        // 추가 제거는 fullSnapshot에 적용됨
        state.zoneInternalID = m_internalID.fetch_add(1);
        std::lock_guard<std::mutex> lock(m_mutex);
        
        uint16_t index = static_cast<uint16_t>(m_chars.size());
        if(index >= MAX_ZONE_CAPACITY)
            return 0;
        auto it = m_sessionToIndex.find(sessionID);
        if (it != m_sessionToIndex.end()) {
            logger->LogError(std::format("ImmigrageChar Failed sessionID already exists in zone {}! sessionID={}", m_zoneID, sessionID));
            return 0;
        }
        state.lastZone = m_zoneID;
        m_chars.push_back(state);
        m_sessionToIndex[sessionID] = index;
        m_InternalIDToIndex[state.zoneInternalID] = index;
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
        m_chars[it->second] = m_chars.back(); // swap

        m_sessionToIndex[m_chars[it->second].sessionID] = it->second;
        m_InternalIDToIndex[m_chars[it->second].zoneInternalID] = it->second;

        m_chars.pop_back();
        m_sessionToIndex.erase(sessionID); 
        m_InternalIDToIndex.erase(o.zoneInternalID); 
        logger->LogInfo("After EmigrateChar, chars.size=" + std::to_string(m_chars.size()));
        return true;
    }

    void ZoneState::DeltaSnapshot() {
        {
            std::lock_guard<std::mutex> lock(m_chatMutex);
            int loop = MAX_CHAT_PACKET;
            if (m_chatQueue.size() > 0) {
                auto packet = writer->GetInitialChatPacket();
                while (loop-- && m_chatQueue.size() > 0) {
                    auto& chat = m_chatQueue.front();
                    auto it = m_sessionToIndex.find(chat.senderSessionID);
                    if (it != m_sessionToIndex.end())
                        writer->WriteChatPacketField(packet, m_chars[it->second].zoneInternalID, chat.message);
                    m_chatQueue.pop();
                }
                logger->LogError(std::format("Chat Packet zone {} Length {} ", m_zoneID, packet->GetLength()));
                broadcast->EnqueueWork(packet, m_zoneID);
            }
        }
        
        std::lock_guard<std::mutex> lock (m_mutex);
        if (m_dirty_list.size() == 0)
            return;
        
        auto packet = writer->GetInitialDeltaPacket();
        int loop = DELTA_UPDATE_COUNT;
        while(loop-- && m_dirty_list.size() > 0)
        {
            auto& session = m_dirty_list.front();
            auto it = m_sessionToIndex.find(session);
            if (it != m_sessionToIndex.end()) {
                auto& character = m_chars[it->second];
                auto& bit = character.dirtyBit;
                if (bit & 0x01)
                    writer->WriteDeltaField(packet, character.zoneInternalID, 0, character.hp);
                if (bit & 0x02)
                    writer->WriteDeltaField(packet, character.zoneInternalID, 1, character.mp);
                if (bit & 0x04)
                    writer->WriteDeltaField(packet, character.zoneInternalID, 2, character.level);
                if (bit & 0x08)
                    writer->WriteDeltaField(packet, character.zoneInternalID, 3, character.exp);
                if (bit & 0x10)
                    writer->WriteDeltaField(packet, character.zoneInternalID, 4, character.dir);
                if (bit & 0x20)
                    writer->WriteDeltaField(packet, character.zoneInternalID, 5, character.x);
                if (bit & 0x40)
                    writer->WriteDeltaField(packet, character.zoneInternalID, 6, character.y);
                character.dirtyBit = 0x00;
            }
            m_dirty_list[0] = m_dirty_list.back();
            m_dirty_list.pop_back();
        }
        broadcast->EnqueueWork(packet, m_zoneID);
    }

    void ZoneState::GetSessionSnapshot(std::vector<uint64_t>& snapshot) {
        std::lock_guard<std::mutex> lock(m_sessionSnapshotMutex);
        snapshot = m_sessionSnapshot;
    }
    void ZoneState::FullSnapshot() {
        {
            std::lock_guard<std::mutex> lock(m_sessionSnapshotMutex);
            m_sessionSnapshot.clear();
            for (auto [session, idx] : m_sessionToIndex)
                m_sessionSnapshot.push_back(session);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_chars.empty())
            return;
        
        auto packet = writer->GetInitialFullPacket();
        for (auto& character: m_chars)
        {
            writer->WriteFullField(packet, character);
        }

        broadcast->EnqueueWork(packet, m_zoneID);
    }

    void ZoneState::Move(uint64_t sessionID, uint8_t dir, float speed) {
        float x = 0;
        float y = 0;
        switch(dir) {
            case 0: y = speed; break;
            case 1: y = -speed; break;
            case 2: x = -speed; break;
            case 3: x = speed; break;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
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
        character.dirtyBit |= 0x10; // dir
        if (x)
            character.dirtyBit |= 0x20; // x
        if (y)
            character.dirtyBit |= 0x40; // y
        m_dirty_list.push_back(sessionID);
    }
}
