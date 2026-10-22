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

#include "IMessageQueue.h"
#include "MessageTypes.h"
#include "Message.h"

#include "ZoneState.h"
#include "Config.h"
#include "IPacket.h"
#include "IIOCP.h"
#include "PacketTypes.h"

namespace Core {
    struct SessionData {
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
        uint64_t lastRespawnRequestEpoch = 0;
        uint64_t bootstrapSequence = 0;
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
        std::shared_mutex smutex;
        std::unordered_map<uint64_t, SessionData> sessionMap;
    };
    class LobbyZone;
    class ChatThreadPool;
    struct ZoneChangeResult {
        uint16_t zoneID = 0;
        uint64_t chatID = 0;
        uint32_t internalID = 0;
        float x = 0, y = 0;
        uint64_t generation = 0;
        uint64_t ownershipEpoch = 0;
    };
    class StateManager {
        std::unordered_map<uint16_t, std::unique_ptr<Core::ZoneState>> m_states;
        std::array<SessionShard, SHARD_SIZE> m_shards; // mutex있어서 vector 사용 불가
        std::atomic<bool> m_running;

        IMessageQueue* mq = nullptr;
        IIOCP* iocp = nullptr;
        LobbyZone* lobbyZone = nullptr;
        ChatThreadPool* chat = nullptr;


        void Initialize(IMessageQueue* m, IIOCP* io, LobbyZone* lobby, ChatThreadPool* c) {
            m_states.reserve(ZONE_COUNT);
            for (int zoneID = 1; zoneID <= ZONE_COUNT; zoneID++) {
                m_states.emplace(zoneID, std::make_unique<ZoneState>(zoneID));
            }
            for (auto& shard : m_shards)
                shard.sessionMap.reserve(MAX_USER_CAPACITY / SHARD_SIZE);
            mq = m;
            lobbyZone = lobby;
            iocp = io;
            chat = c;
            m_running.store(true, std::memory_order_relaxed);
        }

        bool IsReady() {
            if (mq == nullptr) return false;
            if (lobbyZone == nullptr) return false;
            if (chat == nullptr) return false;
            return true;
        }


        void CleanUp();

        friend class Initializer;
        friend class LobbyZone; // ImmigrateChar takes the session shard before the lobby mutex.
        friend class NonZoneThreadPoolFixture;
        friend class ZoneBenchmarkAccess;

    public:
        ~StateManager() {
            CleanUp();
        }

        ZoneState* GetZone(uint16_t zoneID) {
            auto it = m_states.find(zoneID);
            return it != m_states.end() ? it->second.get() : nullptr;
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
            Core::gameLogger->LogInfo("state manager", "AddSession", "session", sessionID, "userID", userID);
            return true;
        }

        uint64_t GetUserID(uint64_t sessionID) {
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
            Core::gameLogger->LogInfo("state manager", "SetCharacterID", "session", sessionID, "characterID", characterID);
        }
        
        uint64_t GetCharacterID(uint64_t sessionID) {
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
        
        int16_t GetZoneID(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
            {
                return -1;
            }
            return it->second.zoneID;
        }

        uint64_t GetFieldCharacterID(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            const auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end() || !it->second.authCompleted
                || !it->second.authenticated || it->second.userID == 0
                || it->second.characterID == 0 || it->second.zoneID == 0
                || it->second.zoneID > ZONE_COUNT)
                return 0;
            return it->second.characterID;
        }

        bool IsFieldSession(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            const auto it = shard.sessionMap.find(sessionID);
            return it != shard.sessionMap.end() && it->second.authCompleted
                && it->second.authenticated && it->second.userID != 0
                && it->second.characterID != 0 && it->second.zoneID > 0
                && it->second.zoneID <= ZONE_COUNT;
        }

        uint8_t HealthCheck(uint64_t sessionID) {
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
                errorLogger->LogError("state manager", "not authenticated", "sessionID", sessionID);
            }
            if (status.cheatCount <= MAX_CHEAT_COUNT) {
                res |= MASK_NOT_CHEAT;
            } else {
                errorLogger->LogError("state manager", "cheat detected", "sessionID", sessionID, "cheatCount", status.cheatCount);
            }
            return res;
        }
        void Disconnect(uint64_t sessionID);
        bool ChangeZone(uint64_t sessionID, uint8_t op, ZoneChangeResult& result);
        bool RespawnToTown(uint64_t sessionID, const RespawnToTownBody& request, ZoneChangeResult& result);
        bool ProcessAction(uint64_t sessionID, uint16_t routeZoneID, const ActionRequestBody& action, ZoneChangeResult& transition);
        bool SendBootstrap(uint64_t sessionID, uint64_t ownershipEpoch);
        void EnqueueDisconnectMsg(CharacterState& temp, uint64_t sessionID);
        void UpdateProfileVersion(uint64_t sessionID, uint32_t version);

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
}
