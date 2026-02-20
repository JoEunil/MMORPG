#include "pch.h"
#include "ZoneState.h"
#include "IPacket.h"
#include "Initializer.h"
#include "ILogger.h"
#include "PacketWriter.h"
#include "StateManager.h"


namespace Core {
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
        m_userCnt.store(m_chars.size(), std::memory_order_relaxed);
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
                        if (character.dirtyBit == 0x0)
                            continue;
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

                perfCollector->AddDeltaFieldCnt(m_zoneID, DELTA_UPDATE_COUNT - loop);
                if (!wroteField) {
                    packets[idx].reset();
                }
            }


        }

        broadcast->EnqueueWork(packets, m_zoneID);
    }

    void ZoneState::FullSnapshot() {
        std::lock_guard<std::mutex> lock(m_mutex);
        UpdateSessionSnapshot();
        perfCollector->UpdateClientCnt(m_zoneID, m_chars.size());
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
                int idx = i * CELLS_X + j;
                for (auto& session : cell.charSessions)
                {
                    auto& character = m_chars[m_sessionToIndex[session]];
                    writer->WriteFullField(packets[idx], character);
                }
            }
        }
        broadcast->EnqueueWork(packets, m_zoneID);
    }

    void ZoneState::DeltaSnapshotMonster() {
        m_deltaTickCounter++;
        if (m_deltaTickCounter & 1) {
            // 2틱 당 한번만 처리
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<IPacket>> packets;
        packets.resize(CELLS_X * CELLS_Y);

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                int idx = i * CELLS_X + j;
                packets[idx] = writer->GetInitialMonsterDeltaPacket();
            }
        }

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                auto& cell = m_cells[i][j];
                int idx = i * CELLS_X + j;
                int loop = DELTA_UPDATE_COUNT*2;

                for (int k =0 ; k < cell.monsterIndexes.size() && loop--; k++)
                {
                    auto& monster = m_monsters[cell.monsterIndexes[k]];
                    auto& bit = monster.dirtyBit;
                    if (bit == 0x00)
                        continue;
                    if (bit & 0x01)
                        writer->WriteMonsterDeltaField(packets[idx], monster.internalID, 0, monster.hp);
                    if (bit & 0x02)
                        writer->WriteMonsterDeltaField(packets[idx], monster.internalID, 1, monster.x);
                    if (bit & 0x04)
                        writer->WriteMonsterDeltaField(packets[idx], monster.internalID, 2, monster.y);
                    if (bit & 0x08)
                        writer->WriteMonsterDeltaField(packets[idx], monster.internalID, 3, monster.dir);
                    monster.dirtyBit = 0x00;
                    monster.attacked = 0;
                }
                perfCollector->AddMonsterDeltaFieldCnt(m_zoneID, DELTA_UPDATE_COUNT * 2 - loop);
            }
        }
        broadcast->EnqueueWork(packets, m_zoneID);
    }

    void ZoneState::FullSnapshotMonster() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<IPacket>> packets;
        packets.resize(CELLS_X * CELLS_Y);
        perfCollector->UpdateMonsterCnt(m_zoneID, m_monsters.size());

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                int idx = i * CELLS_X + j;
                packets[idx] = writer->GetInitialMonsterFullPacket();
            }
        }

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                auto& cell = m_cells[i][j];
                int idx = i * CELLS_X + j;

                for (auto& monsterIdx : cell.monsterIndexes)
                {
                    if (m_monsters[monsterIdx].hp == 0)
                        continue;
                    writer->WriteMonsterFullField(packets[idx], m_monsters[monsterIdx]);
                    m_monsters[monsterIdx].attacked = 0;
                    m_monsters[monsterIdx].dirtyBit = 0x0;
                }
            }
        }
        broadcast->EnqueueWork(packets, m_zoneID);
    }

    void ZoneState::ActionSnapshot() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<IPacket>> packets;
        packets.resize(CELLS_X * CELLS_Y);

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                int idx = i * CELLS_X + j;
                packets[idx] = writer->GetInitialActionPacket();
            }
        }

        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                bool wroteField = false;
                auto& cell = m_cells[i][j];
                int idx = i * CELLS_X + j;
                perfCollector->AddActionFieldCnt(m_zoneID, cell.actionResults.size());
                for (auto& actionRes: cell.actionResults)
                {
                    writer->WriteActionField(packets[idx], actionRes);
                    wroteField = true;
                }
                cell.actionResults.clear();
                if (!wroteField)
                    packets[idx].reset();
            }
        }
        broadcast->EnqueueWork(packets, m_zoneID);
    }
}
