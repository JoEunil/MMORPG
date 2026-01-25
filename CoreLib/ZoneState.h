#pragma once


#include <shared_mutex>
#include <cstdint>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>

#include "Config.h"

namespace Core {
    struct alignas(64) CharacterState {
        uint64_t zoneInternalID; // zone 내부에서 사용하는 id
        int hp; // 0
        int mp; // 1
        uint16_t level; // 2
        uint32_t exp; // 3
        uint8_t dir = 0;  // 4
        float x, y; // 5, 6
        
        // -- full State에만
        uint16_t lastZone;
        char charName[MAX_CHARNAME_LEN];
        // 장비, 룬, 스킬 등 실시간 로직이 들어감
        // 장비, 룬, 스킬 변경 등은 비시뮬레이션 작업으로, noneZoneHandler를 통해 이루어짐
        // 캐시가 무효화되어도 해당 로직의 빈도가 게임 틱에 비하면 무시될 수준
        
        // --  내부 정보
        uint8_t dirtyBit = 0x00; // 변경된 필드 표시
        uint64_t sessionID;  
        uint64_t characterID;
    };

    class IPacket;
    class ILogger;
    class BroadcastThreadPool;
    class PacketWriter;
    class StateManager;

    struct ZoneArea {
        float x_min, x_max;
        float y_min, y_max;
    };

    class ZoneState {
        std::vector<CharacterState> m_chars; // 캐시 지역성, 연속된 메모리 공간을 사용하는 vector 사용.
        std::unordered_map<uint64_t, uint16_t> m_sessionToIndex; // 자신의 캐릭터를 컨트롤 할 때
        std::unordered_map<uint64_t, uint16_t> m_InternalIDToIndex; // 다른 캐릭터와 상호작용할 때
        std::vector<uint64_t> m_dirty_list; // session 담기
        std::vector<std::pair<uint64_t, uint16_t>> m_cheatList;  // Cheat 탐지해서 배치처리하는 용도, stack
        std::mutex m_mutex;
        
        std::vector<uint64_t> m_sessionSnapshot;
        std::mutex m_sessionSnapshotMutex;

        std::atomic<uint64_t> m_internalID = 1; // 각각 character에 ID 부여
        uint16_t m_zoneID;
        ZoneArea m_area;

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
        friend class Initializer;
    public:
        ZoneState(uint16_t zone) {
            m_zoneID = zone;
            m_chars.reserve(MAX_ZONE_CAPACITY);
            m_dirty_list.reserve(MAX_ZONE_CAPACITY);
            m_sessionSnapshot.reserve(MAX_ZONE_CAPACITY);
            m_sessionToIndex.reserve(MAX_ZONE_CAPACITY);
            m_InternalIDToIndex.reserve(MAX_ZONE_CAPACITY);
            m_cheatList.reserve(MAX_ZONE_CAPACITY);

            int x = (zone-1) % ZONE_HORIZON;
            int y = (zone-1) / ZONE_HORIZON;
            m_area.x_min = x * ZONE_SIZE - TRANSITION_BUFFER;
            m_area.x_max = (x + 1) * ZONE_SIZE + TRANSITION_BUFFER;
            m_area.y_min = y * ZONE_SIZE - TRANSITION_BUFFER;
            m_area.y_max = (y + 1) * ZONE_SIZE + TRANSITION_BUFFER;
        }

        uint64_t ImmigrateChar(uint64_t sessionID, CharacterState& user);
        bool EmigrateChar(uint64_t sessionID, CharacterState& o);
        void GetSessionSnapshot(std::vector<uint64_t>& snapshot);
        void DeltaSnapshot();
        void FullSnapshot();
        void Move(uint64_t sessionID, uint8_t dir, float speed);
        void FlushCheat();
    };
}
