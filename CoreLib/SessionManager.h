#pragma once

#include <vector>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <array>
#include <chrono>
#include <memory>
#include <new>

#include "ZoneState.h"
#include "WorldConfig.h"
#include "PacketConfig.h"
#include "IPacket.h"
#include "IInventoryOperations.h"
#include "INetworkSender.h"
#include "PacketTypes.h"

namespace Core {
    struct SessionData {
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
        std::shared_ptr<FieldZoneTransfer> pendingFieldTransfer;
        uint64_t lastRespawnRequestEpoch = 0;
        uint64_t bootstrapSequence = 0;
        ItemUseResponseBody lastItemUse = {};
        ParryResponseBody lastParry = {};
        EquipmentResponseBody lastEquipmentSet = {};
        uint32_t lastEquipmentItemID = 0;
        uint64_t chatID = 0;
        uint64_t userID = 0;
        uint64_t characterID = 0;
        uint16_t cheatCount = 0;
        //uint64_t guildID = 0;  
        //uint64_t partyID = 0; 
        // session의 guild, party는 여기서 1차로 관리. 
        std::chrono::steady_clock::time_point lastCheatTime;
        bool authenticated = false;
        bool authCompleted = false; // Pending is distinct from a completed failure.
    };

    struct alignas(std::hardware_destructive_interference_size) SessionShard {
        mutable std::shared_mutex smutex;
        std::unordered_map<uint64_t, SessionData> sessionMap;
    };
    class CacheCommandSender;
    class LobbyZone;
    class ChatThreadPool;
    class ZoneEventRouter;
    struct FieldZoneTransfer;
    struct ProfileVersionUpdate;
    struct ZoneChangeResult;
    class SessionManager {
        std::array<SessionShard, SHARD_SIZE> m_shards; // mutex있어서 vector 사용 불가
        std::atomic<bool> m_running;

        IInventoryOperations* inventory = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        INetworkSender* networkSender = nullptr;
        LobbyZone* lobbyZone = nullptr;
        ChatThreadPool* chat = nullptr;
        ZoneEventRouter* zoneRouter = nullptr;


        void Initialize(const CacheCommandSender* commands, INetworkSender* sender, LobbyZone* lobby, ChatThreadPool* c,
            ZoneEventRouter& router) {
            for (auto& shard : m_shards)
                shard.sessionMap.reserve(MAX_USER_CAPACITY / SHARD_SIZE);
            cacheCommands = commands;
            lobbyZone = lobby;
            networkSender = sender;
            chat = c;
            zoneRouter = &router;
            m_running.store(true, std::memory_order_relaxed);
        }

        bool IsReady() const {
            if (zoneRouter == nullptr) {
                sysLogger->LogError("core session manager", "zone router not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("core session manager", "cache command sender not initialized");
                return false;
            }
            if (lobbyZone == nullptr) {
                sysLogger->LogError("core session manager", "lobby zone not initialized");
                return false;
            }
            if (chat == nullptr) {
                sysLogger->LogError("core session manager", "chat thread pool not initialized");
                return false;
            }
            return true;
        }


        void CleanUp();
        void EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character);
        static void UpdateZoneOwnershipLocked(SessionData& session, uint16_t destinationID);
        bool DescribeZoneChangeLocked(uint64_t sessionID, const SessionData& session, ZoneChangeResult& result);
        void RouteProfileVersionLocked(const SessionData& session, const ProfileVersionUpdate& update);

        friend class Initializer;
        friend class LobbyZone; // ImmigrateChar takes the session shard before the lobby mutex.
        friend class ZoneBenchmarkAccess;
        friend class ZoneHandler;

    public:
        ~SessionManager() {
            CleanUp();
        }

        void BeginAuth(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            // The worker still owns the AUTH PacketView here.
            shard.sessionMap.try_emplace(sessionID);
        }

        bool CompleteAuth(uint64_t sessionID, uint64_t userID, bool authenticated) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return false; // Disconnect already removed the pending entry.
            if (it->second.authCompleted)
                return true; // Preserve first-completion-wins state, including failure.
            it->second.userID = userID;
            it->second.zoneID = 0;
            it->second.lastRespawnRequestEpoch = 0;
            it->second.characterID = 0;
            it->second.cheatCount = 0;
            it->second.lastCheatTime = std::chrono::steady_clock::now();
            it->second.authenticated = authenticated;
            it->second.authCompleted = true;
            Core::gameLogger->LogInfo("core session manager", "AddSession", "session", sessionID, "userID", userID);
            return true;
        }

        uint64_t GetUserID(uint64_t sessionID) const {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return 0;
            return it->second.userID;
        }

        void SetCharacterID(uint64_t sessionID, uint64_t characterID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return ;
            it->second.characterID = characterID;
            Core::gameLogger->LogInfo("core session manager", "SetCharacterID", "session", sessionID, "characterID", characterID);
        }
        
        uint64_t GetCharacterID(uint64_t sessionID) const {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return 0;
            return it->second.characterID;
        }
        
        void SetZoneID(uint64_t sessionID, uint16_t dest) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return ;
            it->second.zoneID = dest;
        }
        
        int16_t GetZoneID(uint64_t sessionID) const {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
            {
                return -1;
            }
            return it->second.zoneID;
        }

        uint64_t GetFieldCharacterID(uint64_t sessionID) const {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            const auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end() || !it->second.authCompleted
                || !it->second.authenticated || it->second.userID == 0
                || it->second.characterID == INVALID_CHARACTER_ID || it->second.zoneID == 0
                || it->second.zoneID > ZONE_COUNT)
                return INVALID_CHARACTER_ID;
            return it->second.characterID;
        }

        bool IsFieldSession(uint64_t sessionID) const {
            return GetFieldCharacterID(sessionID) != INVALID_CHARACTER_ID;
        }

        uint8_t HealthCheck(uint64_t sessionID) const {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            uint8_t res = 0; 
            if (it == shard.sessionMap.end())
            {
                return res;
            }

            auto status = it->second;
            lock.unlock();

            res |= MASK_EXIST;
            if (status.authenticated) {
                res |= MASK_AUTHENTICATED;
            } else {
                errorLogger->LogError("core session manager", "not authenticated", "sessionID", sessionID);
            }
            if (status.cheatCount <= MAX_CHEAT_COUNT) {
                res |= MASK_NOT_CHEAT;
            } else {
                errorLogger->LogError("core session manager", "cheat detected", "sessionID", sessionID, "cheatCount", status.cheatCount);
            }
            return res;
        }
        ItemUseResponseBody UseItem(uint64_t sessionID, const ItemUseRequestBody& request);
        EquipmentResponseBody Equipment(uint64_t sessionID, uint64_t requestID, uint64_t ownershipEpoch, bool write, uint8_t slot = 0, uint32_t itemID = 0);
        ParryResponseBody Parry(uint64_t sessionID, uint16_t routeZoneID, const ParryRequestBody& request);
        void GrantMonsterReward(const CharacterState& character, uint64_t gold);
        void Disconnect(uint64_t sessionID);
        bool ChangeZone(uint64_t sessionID, uint8_t op, ZoneChangeResult& result);
        bool RespawnToTown(uint64_t sessionID, const RespawnToTownBody& request, ZoneChangeResult& result);
        bool SendBootstrap(uint64_t sessionID, uint64_t ownershipEpoch);
        void QueueProfileVersionUpdate(const ProfileVersionUpdate& update);

        void Cheat(uint64_t sessionID, uint16_t cheat, std::chrono::steady_clock::time_point time) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it != shard.sessionMap.end())
            {
                auto before = it->second.lastCheatTime;
                it->second.lastCheatTime = time;
                if (it->second.lastCheatTime - before > CHEAT_FLUSH_TIME) {
                    it->second.cheatCount = 0;
                }
                it->second.cheatCount += cheat;
            }
        }
    };

    struct ZoneChangeResult {
        uint16_t zoneID = 0;
        uint64_t chatID = 0;
        uint32_t internalID = 0;
        float x = 0, y = 0;
        uint64_t generation = 0;
        uint64_t ownershipEpoch = 0;
    };
}
