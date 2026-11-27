#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <array>
#include <atomic>
#include <optional>

#include <new>

#include "CharacterState.h"
#include "Cell.h"
#include "MonsterState.h"
#include "WorldConfig.h"
#include "../Packet/PacketConfig.h"
#include "../Packet/PacketTypes.h"
#include "../Data/SkillData.h"
#include "../CorePerfCollector.h"
#include "Interface/ISessionSnapshotReader.h"
#include <CoreLib/LoggerGlobal.h>

namespace Core {
    class IPacket;
    class BroadcastThreadPool;
    class PacketWriter;
    class SessionManager;
    class IZoneTransfer;
    class CacheCommandSender;
    class IServerResponseQueue;
    class IInventoryOperations;
    struct ItemUseRequestBody;
    struct ItemUseResponseBody;
    struct EquipmentResponseBody;
    struct ActionRequestBody;
    struct ParryRequestBody;
    struct ParryResponseBody;
    struct ZoneChangeResult;

    struct SkillRequest {
        uint64_t sessionID;
        uint8_t skillSlot;
        const Data::SkillData& skillInfo;
    };

    struct ZoneTransfer {
        enum class Kind {
            Movement,
            Respawn
        };

        CharacterState character{};
        uint64_t ownershipEpoch = 0;
        uint16_t sourceZoneID = 0;
        uint8_t dir = 0;
        uint8_t skillSlot = 0;
        float x = 0, y = 0, moveBudget = 0, aimAngle = 0;
        Kind kind = Kind::Movement;
        bool IsRespawn() const { return kind == Kind::Respawn; }
    };

    // Mutable state belongs to the Zone worker; cross-thread readers use published snapshots.
    // Construction and destruction occur outside the worker lifetime.
    class ZoneState {
        std::vector<CharacterState> m_chars;
        std::unordered_map<uint64_t, uint16_t> m_sessionToIndex;    // 자신의 캐릭터를 컨트롤 할 때
        std::unordered_map<uint32_t, uint16_t> m_InternalIDToIndex; // 다른 캐릭터와 상호작용할 때
        std::vector<MonsterState> m_monsters;                       // id = index, 죽은 몬스터는 제거하지 않고 틱 단위로 처리.
        std::vector<std::pair<uint64_t, uint16_t>> m_cheatList;     // Cheat 탐지해서 배치처리하는 용도, stack

        uint64_t m_deltaTickCounter;
        bool m_monsterCellsChanged = false;
        uint16_t m_zoneID;
        uint64_t m_generation = 0; // Advanced by successful registrations on the Zone worker.
        ZoneArea m_area;
        std::array<std::array<Cell, CELLS_X>, CELLS_Y> m_cells;

        alignas(std::hardware_destructive_interference_size) std::atomic<uint32_t> m_internalIdGenerator = 1; // 각각 character에 ID 부여
        alignas(std::hardware_destructive_interference_size) std::atomic<uint32_t> m_userCnt;
        char padding[std::hardware_destructive_interference_size - sizeof(uint32_t)];

        Base::TripleBuffer<SessionSnapshot> tripleBuffer;
        SessionSnapshot* sessionSnapshotWriter;

        inline static BroadcastThreadPool* broadcast;
        inline static PacketWriter* writer;
        inline static SessionManager* sessionManager;
        inline static const CacheCommandSender* cacheCommands = nullptr;
        inline static IServerResponseQueue* responses = nullptr;
        inline static IInventoryOperations* inventory = nullptr;
        inline static CorePerfCollector* perfCollector;
        static void Initialize(BroadcastThreadPool* b, PacketWriter* p, SessionManager* s, CorePerfCollector* pc, const CacheCommandSender& commands,
            IServerResponseQueue& responseQueue, IInventoryOperations* items = nullptr) {
            broadcast = b;
            writer = p;
            sessionManager = s;
            cacheCommands = &commands;
            responses = &responseQueue;
            inventory = items;
            perfCollector = pc;
        }
        static bool IsReady() {
            if (broadcast == nullptr) {
                sysLogger->LogError("zone state", "broadcast not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("zone state", "writer not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("zone state", "cache command sender not initialized");
                return false;
            }
            if (responses == nullptr) {
                sysLogger->LogError("zone state", "response queue not initialized");
                return false;
            }
            return true;
        }

        CharacterState* FindCharacter(uint64_t sessionID) {
            const auto found = m_sessionToIndex.find(sessionID);
            return found == m_sessionToIndex.end() ? nullptr : &m_chars[found->second];
        }
        void RemoveFromCell(CharacterState& character);
        void AddToCell(CharacterState& character, uint8_t x, uint8_t y);
        uint32_t ImmigrateChar(uint64_t sessionID, const CharacterState& user);
        std::optional<uint32_t> ValidateCharacterEntry(uint64_t sessionID, const CharacterState& state) const;
        CharacterState PrepareCharacterForEntry(uint64_t sessionID, uint32_t internalID, const CharacterState& state) const;
        bool RegisterCharacter(CharacterState&& incoming);
        void ClearCharacterEffects(uint64_t sessionID, uint32_t internalID);
        bool AdmitZoneTransfer(const ZoneTransfer& transfer);
        bool RestoreZoneTransfer(const CharacterState& original);
        bool TryStartZoneTransfer(uint16_t destination, const std::shared_ptr<ZoneTransfer>& transfer, IZoneTransfer& zoneTransfer);
        void RemoveCharacter(uint64_t sessionID);
        void RestoreMovement(uint64_t sessionID, float x, float y, uint8_t dir, float moveBudget, uint32_t dirtyBit);
        void UpdateSessionSnapshot();
        void MoveMonsterHome(MonsterState& monster);
        bool TryResolveSkillCaster(ActiveSkill& skill, std::optional<std::reference_wrapper<CharacterState>>& caster);
        void ApplyPlayerHit(CharacterState& caster, const ActiveSkill& skill, const Data::SkillData& skillData, int idx);
        void ApplyMonsterHit(const ActiveSkill& skill, const Data::SkillData& skillData, int idx);
        void GrantMonsterReward(const CharacterState& character, uint64_t gold);
        void UseMonsterSkill(MonsterState& monster);
        void SetMonsterPosition(MonsterState& monster, float x, float y);
        bool MoveMonster(MonsterState& monster, float x, float y);
        friend class Initializer;
        friend class ZoneBenchmarkAccess;
        friend class MonsterBehaviorFixture;

      public:
        explicit ZoneState(uint16_t zone) {
            m_zoneID = zone;
            m_chars.reserve(MAX_ZONE_CAPACITY);
            m_sessionToIndex.reserve(MAX_ZONE_CAPACITY);
            m_InternalIDToIndex.reserve(MAX_ZONE_CAPACITY);
            m_cheatList.reserve(MAX_ZONE_CAPACITY);
            m_deltaTickCounter = 0;

            ZoneInit(m_area, m_zoneID);
            CellInit(m_cells, m_area);
            m_monsters.reserve(MAX_MONSTER_COUNT);
            m_userCnt.store(0, std::memory_order_relaxed);
            InitAOI();

            const auto allocateSnapshot = [] {
                auto snapshot = std::make_unique<SessionSnapshot>(CELLS_X * CELLS_Y);
                for (auto& cell : *snapshot)
                    cell.reserve(CELL_CAPACITY);
                return snapshot;
            };
            auto back1 = allocateSnapshot();
            auto back2 = allocateSnapshot();
            sessionSnapshotWriter = allocateSnapshot().release();
            tripleBuffer.Init(back1.release(), back2.release());
        }
        ~ZoneState() { delete sessionSnapshotWriter; }
        uint16_t GetZoneID() const { return m_zoneID; }

        void ApplyAction(uint64_t sessionID, const ActionRequestBody& action, IZoneTransfer& zoneTransfer);
        ParryResponseBody Parry(uint64_t sessionID, const ParryRequestBody& request);
        bool AdmitLobbyCharacter(uint64_t sessionID, CharacterState incoming, ZoneChangeResult& result);
        bool DescribeOrRespawnInTown(uint64_t sessionID, uint64_t characterID, bool replay, ZoneChangeResult& result);
        bool StartRespawnTransfer(uint64_t sessionID, uint64_t characterID, uint64_t ownershipEpoch, IZoneTransfer& zoneTransfer);
        bool TryResolveZoneTransfer(const ZoneTransfer& transfer, bool returning, ZoneChangeResult& result);
        void UpdateProfileVersion(uint64_t sessionID, uint32_t profileID, uint32_t version);
        bool DescribeZoneChange(uint64_t sessionID, uint64_t chatID, uint64_t ownershipEpoch, ZoneChangeResult& result);
        bool EnqueueBootstrap(uint64_t sessionID, uint64_t ownershipEpoch, uint64_t bootstrapID);

        std::optional<ItemUseResponseBody> UseItem(uint64_t sessionID, const ItemUseRequestBody& request);
        std::optional<EquipmentResponseBody> Equipment(uint64_t sessionID, uint64_t requestID, uint64_t ownershipEpoch, bool write, uint8_t slot,
            uint32_t itemID);
        bool TakeCharacter(CharacterState& out);
        void InitializeMonster();
        bool EmigrateChar(uint64_t sessionID, CharacterState& o);
        SessionSnapshotView ReadSessionSnapshot() { return tripleBuffer.Read(); }
        void DeltaSnapshot();
        void DeltaSnapshotMonster();
        void FullSnapshot();
        void FullSnapshotMonster();
        void ActionSnapshot();
        void ReplenishMoveBudget();
        void Move(uint64_t sessionID, uint8_t dir, float x, float y);
        void Skill(uint64_t sessionID, uint8_t skillSlot, float aimAngle);
        void DirtyCheck(uint64_t sessionID);
        void FlushCheat();
        void UpdateMonster();
        void SkillCoolDown();
        void ApplySkill();
        void PopSkill(int idx, std::vector<ActiveSkill>& list);
        void ProcessCellActiveSkills(Cell& cell);
        void ApplyHit(std::optional<std::reference_wrapper<CharacterState>> caster, ActiveSkill& skill, int idx);
        void MoveToward(MonsterState& monster, CharacterState& character);
        void MoveAround(MonsterState& monster);
        uint32_t GetUserCnt() const { return m_userCnt.load(std::memory_order_relaxed); }
    };
}
