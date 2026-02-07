#pragma once


#include <shared_mutex>
#include <cstdint>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <array>

#include "CharacterState.h"
#include "ZoneArea.h"
#include "MonsterState.h"
#include "Config.h"

#include <BaseLib/TripleBufferAdvanced.h>

namespace Core {
    class IPacket;
    class ILogger;
    class BroadcastThreadPool;
    class PacketWriter;
    class StateManager;
    class ZoneState {
        std::vector<CharacterState> m_chars;
        std::unordered_map<uint64_t, uint16_t> m_sessionToIndex; // 자신의 캐릭터를 컨트롤 할 때
        std::unordered_map<uint64_t, uint16_t> m_InternalIDToIndex; // 다른 캐릭터와 상호작용할 때

        std::vector<MonsterState> m_monsters; // id = index, 죽은 몬스터는 제거하지 않고 틱 단위로 처리.

        std::vector<std::pair<uint64_t, uint16_t>> m_cheatList;  // Cheat 탐지해서 배치처리하는 용도, stack
        std::mutex m_mutex;

        std::atomic<uint64_t> m_internalIdGenerator= 1; // 각각 character에 ID 부여
        uint16_t m_zoneID;
        ZoneArea m_area;

        std::array<std::array<Cell, CELLS_X>, CELLS_Y> m_cells;

        Base::TripleBufferAdvanced<std::vector<std::vector<uint64_t>>> tripleBuffer;
        std::vector<std::vector<uint64_t>>* sessionSnapshotWriter;

        inline static BroadcastThreadPool* broadcast;
        inline static ILogger* logger;
        inline static PacketWriter* writer;
        inline static StateManager* stateManager;
        static void Initialize(ILogger* l, BroadcastThreadPool* b, PacketWriter* p, StateManager* s) {
            logger = l;
            broadcast = b;
            writer = p;
            stateManager = s;
        }
        static bool IsReady() {
            if (broadcast == nullptr)
                return false;
            if(writer == nullptr)
                return false;
            return true;
        }

        void RemoveFromCell(CharacterState& character);
        void AddToCell(CharacterState& character,  uint8_t x, uint8_t y);
        void UpdateSessionSnapshot();
        friend class Initializer;
    public:
        ZoneState(uint16_t zone) {
            m_zoneID = zone;
            m_chars.reserve(MAX_ZONE_CAPACITY);
            m_sessionToIndex.reserve(MAX_ZONE_CAPACITY);
            m_InternalIDToIndex.reserve(MAX_ZONE_CAPACITY);
            m_cheatList.reserve(MAX_ZONE_CAPACITY);

            ZoneInit(m_area, m_zoneID);
            CellInit(m_cells, m_area);
            auto back1 = new std::vector<std::vector<uint64_t>>(CELLS_X * CELLS_Y);
            auto back2 = new std::vector<std::vector<uint64_t>>(CELLS_X * CELLS_Y);
            sessionSnapshotWriter = new std::vector<std::vector<uint64_t>>(CELLS_X * CELLS_Y);
            for (auto& cell : *back1) {
                cell.reserve(CELL_CAPACITY); 
            }
            for (auto& cell : *back2) {
                cell.reserve(CELL_CAPACITY);
            }
            for (auto& cell : *sessionSnapshotWriter) {
                cell.reserve(CELL_CAPACITY);
            }
            tripleBuffer.Init(back1, back2);
        }

        uint64_t ImmigrateChar(uint64_t sessionID, CharacterState& user);
        bool EmigrateChar(uint64_t sessionID, CharacterState& o);
        Base::BufferReader<std::vector<std::vector<uint64_t>>> GetSessionSnaphot() {
            return tripleBuffer.Read();
        }
        void DeltaSnapshot();
        void FullSnapshot();
        bool Move(uint64_t sessionID, uint8_t dir, float speed);
        void DirtyCheck(uint64_t sessionID);
        void FlushCheat();
        void UpdateMonster();
    };
}
