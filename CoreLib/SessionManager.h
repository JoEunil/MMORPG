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
#include <optional>

#include "ZoneState.h"
#include "WorldConfig.h"
#include "PacketConfig.h"
#include "IPacket.h"
#include "IInventoryOperations.h"
#include "ISessionStatusReader.h"
#include "IZoneRequestQueue.h"
#include "INetworkSender.h"
#include "PacketTypes.h"

namespace Core {
    struct SessionData {
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
        std::shared_ptr<ZoneTransfer> pendingZoneTransfer;
        uint64_t lastRespawnRequestEpoch = 0;
        uint64_t bootstrapSequence = 0;
        ItemUseResponseBody lastItemUse = {};
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
        bool disconnectQueued = false;
    };

    struct alignas(std::hardware_destructive_interference_size) SessionShard {
        mutable std::shared_mutex smutex;
        std::unordered_map<uint64_t, SessionData> sessionMap;
    };
    class CacheCommandSender;
    class LobbyZone;
    class ChatThreadPool;
    class ZoneEventRouter;
    struct ZoneTransfer;
    struct ZoneChangeResult;
    enum class ZoneOperationResult { NoResponse, Rejected, Completed };
    class SessionManager : public ISessionStatusReader {
        std::array<SessionShard, SHARD_SIZE> m_shards; // mutex있어서 vector 사용 불가
        std::atomic<bool> m_running;

        IInventoryOperations* inventory = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        INetworkSender* networkSender = nullptr;
        LobbyZone* lobbyZone = nullptr;
        ChatThreadPool* chat = nullptr;
        ZoneEventRouter* zoneRouter = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;


        void Initialize(const CacheCommandSender* commands, INetworkSender* sender, LobbyZone* lobby, ChatThreadPool* c,
            ZoneEventRouter& router, IZoneRequestQueue& requests) {
            for (auto& shard : m_shards)
                shard.sessionMap.reserve(MAX_USER_CAPACITY / SHARD_SIZE);
            cacheCommands = commands;
            lobbyZone = lobby;
            networkSender = sender;
            chat = c;
            zoneRouter = &router;
            zoneRequests = &requests;
            m_running.store(true, std::memory_order_relaxed);
        }

        bool IsReady() const {
            if (zoneRequests == nullptr) {
                sysLogger->LogError("core session manager", "zone request queue not initialized");
                return false;
            }
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
        static void UpdateZoneOwnershipLocked(SessionData& session, uint16_t destinationID);
        bool QueueBootstrapLocked(uint64_t sessionID, const SessionData& session, bool resendZoneChange);
        static uint64_t NextBootstrapIDLocked(SessionData& session);
        void RouteProfileVersionLocked(const SessionData& session, const ProfileVersionUpdate& update);
        static SessionData* FindSessionLocked(SessionShard& shard, uint64_t sessionID) {
            const auto found = shard.sessionMap.find(sessionID);
            return found != shard.sessionMap.end() ? &found->second : nullptr;
        }
        static bool MatchesZoneTransfer(const SessionData* session, const std::shared_ptr<ZoneTransfer>& transfer);
        void JoinZoneChatLocked(uint64_t sessionID, uint16_t zoneID, SessionData& session,
            std::optional<uint32_t> lobbyProfile = std::nullopt);
        void LeaveZoneChatLocked(uint64_t sessionID, uint16_t zoneID);

        friend class Initializer;
        friend class ZoneBenchmarkAccess;

    public:
        ~SessionManager() override {
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
        
        int16_t GetZoneID(uint64_t sessionID) const override {
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

        uint8_t HealthCheck(uint64_t sessionID) const override {
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
        void GrantMonsterReward(const CharacterState& character, uint64_t gold);
        void RequestDisconnect(uint64_t sessionID);
        bool CompleteZoneDisconnect(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
            std::shared_ptr<ZoneTransfer>& pending);
        bool QueueLobbyEntry(uint64_t sessionID, uint8_t op);
        bool QueueRespawnToTown(uint64_t sessionID, uint64_t ownershipEpoch);
        bool QueueBootstrap(uint64_t sessionID, uint64_t ownershipEpoch);
        void QueueProfileVersionUpdate(const ProfileVersionUpdate& update);

        // Callbacks run under the session shard lock. They may acquire the
        // destination state lock, but must not reenter SessionManager.
        template<class Apply>
        void ApplyZoneAction(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch, Apply apply);
        template<class Store>
        bool LoadLobbyCharacter(uint64_t sessionID, uint64_t characterID, Store store);
        template<class Admit>
        ZoneOperationResult EnterZoneFromLobby(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
            ZoneChangeResult& result, Admit admit);
        template<class StartTransfer, class RespawnInTown>
        ZoneOperationResult RespawnInZone(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
            bool townSpawnWalkable, ZoneChangeResult& result, StartTransfer startTransfer, RespawnInTown respawnInTown);
        template<class Admit, class AfterArrival>
        ZoneOperationResult ResolveZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t zoneID, bool returning,
            ZoneChangeResult& result, Admit admit, AfterArrival afterArrival);
        template<class Store>
        uint64_t ReturnTransferToLobby(const std::shared_ptr<ZoneTransfer>& transfer, Store store);
        template<class Apply>
        void ApplyZoneProfileVersion(uint16_t zoneID, const ProfileVersionUpdate& update, Apply apply);
        template<class Describe, class Enqueue>
        void ProcessZoneBootstrap(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch, bool resendZoneChange,
            Describe describe, Enqueue enqueue);
        template<class Parry>
        ParryResponseBody ApplyZoneParry(uint64_t sessionID, uint16_t zoneID, const ParryRequestBody& request, Parry parry);

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

    template<class Apply>
    void SessionManager::ApplyZoneAction(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch, Apply apply) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted
            || session->zoneID == 0 || session->ownershipEpoch == 0 || session->pendingZoneTransfer)
            return;
        if (session->zoneID != zoneID || session->ownershipEpoch != ownershipEpoch) {
            QueueBootstrapLocked(sessionID, *session, true);
            return;
        }
        session->lastRespawnRequestEpoch = 0;
        session->pendingZoneTransfer = apply();
    }

    template<class Store>
    bool SessionManager::LoadLobbyCharacter(uint64_t sessionID, uint64_t characterID, Store store) {
        if (characterID == INVALID_CHARACTER_ID)
            return false;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || !session->authenticated || session->zoneID != 0
            || (session->characterID != INVALID_CHARACTER_ID && session->characterID != characterID)
            || !store())
            return false;
        session->characterID = characterID;
        return true;
    }

    template<class Admit>
    ZoneOperationResult SessionManager::EnterZoneFromLobby(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
        ZoneChangeResult& result, Admit admit) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || !session->authenticated || !session->authCompleted
            || session->zoneID != 0 || session->ownershipEpoch != ownershipEpoch)
            return ZoneOperationResult::NoResponse;
        const auto profile = admit(session->characterID);
        if (!profile)
            return ZoneOperationResult::Rejected;

        UpdateZoneOwnershipLocked(*session, zoneID);
        JoinZoneChatLocked(sessionID, zoneID, *session, profile);
        result.chatID = session->chatID;
        result.ownershipEpoch = session->ownershipEpoch;
        return ZoneOperationResult::Completed;
    }

    template<class StartTransfer, class RespawnInTown>
    ZoneOperationResult SessionManager::RespawnInZone(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
        bool townSpawnWalkable, ZoneChangeResult& result, StartTransfer startTransfer, RespawnInTown respawnInTown) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted || session->zoneID != zoneID)
            return ZoneOperationResult::NoResponse;
        if (session->pendingZoneTransfer && session->pendingZoneTransfer->IsRespawn()
            && session->ownershipEpoch == ownershipEpoch)
            return ZoneOperationResult::NoResponse;

        const bool inTown = zoneID == TOWN_ZONE_ID;
        const bool replay = session->ownershipEpoch != ownershipEpoch;
        const bool canReplay = inTown && session->lastRespawnRequestEpoch == ownershipEpoch
            && session->ownershipEpoch == ownershipEpoch + 1;
        if (session->pendingZoneTransfer || (replay && !canReplay) || !townSpawnWalkable)
            return ZoneOperationResult::Rejected;

        if (!inTown) {
            session->pendingZoneTransfer = startTransfer(session->characterID, session->ownershipEpoch);
            return session->pendingZoneTransfer ? ZoneOperationResult::NoResponse : ZoneOperationResult::Rejected;
        }

        if (!respawnInTown(session->characterID, replay))
            return ZoneOperationResult::Rejected;
        if (!replay) {
            session->lastRespawnRequestEpoch = ownershipEpoch;
            UpdateZoneOwnershipLocked(*session, TOWN_ZONE_ID);
        }
        result.chatID = session->chatID;
        result.ownershipEpoch = session->ownershipEpoch;
        return ZoneOperationResult::Completed;
    }

    template<class Admit, class AfterArrival>
    ZoneOperationResult SessionManager::ResolveZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t zoneID,
        bool returning, ZoneChangeResult& result, Admit admit, AfterArrival afterArrival) {
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!MatchesZoneTransfer(session, transfer) || returning != (zoneID == transfer->sourceZoneID))
            return ZoneOperationResult::NoResponse;
        if (!admit())
            return ZoneOperationResult::Rejected;

        session->pendingZoneTransfer.reset();
        if (!returning) {
            if (transfer->IsRespawn())
                session->lastRespawnRequestEpoch = transfer->ownershipEpoch;
            UpdateZoneOwnershipLocked(*session, zoneID);
            result.chatID = session->chatID;
            result.ownershipEpoch = session->ownershipEpoch;
            JoinZoneChatLocked(sessionID, zoneID, *session);
            afterArrival();
        }
        return ZoneOperationResult::Completed;
    }

    template<class Store>
    uint64_t SessionManager::ReturnTransferToLobby(const std::shared_ptr<ZoneTransfer>& transfer, Store store) {
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!MatchesZoneTransfer(session, transfer) || !store())
            return 0;

        UpdateZoneOwnershipLocked(*session, 0);
        session->pendingZoneTransfer.reset();
        LeaveZoneChatLocked(sessionID, transfer->sourceZoneID);
        return session->ownershipEpoch;
    }

    template<class Apply>
    void SessionManager::ApplyZoneProfileVersion(uint16_t zoneID, const ProfileVersionUpdate& update, Apply apply) {
        auto& shard = m_shards[update.sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, update.sessionID);
        if (!session)
            return;
        if (session->zoneID != zoneID || session->pendingZoneTransfer) {
            RouteProfileVersionLocked(*session, update);
            return;
        }
        apply();
    }

    template<class Describe, class Enqueue>
    void SessionManager::ProcessZoneBootstrap(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
        bool resendZoneChange, Describe describe, Enqueue enqueue) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || session->zoneID != zoneID || session->ownershipEpoch != ownershipEpoch
            || session->pendingZoneTransfer)
            return;
        if (resendZoneChange) {
            describe(session->chatID);
            return;
        }
        const auto bootstrapID = NextBootstrapIDLocked(*session);
        if (bootstrapID != 0)
            enqueue(bootstrapID);
    }

    template<class Parry>
    ParryResponseBody SessionManager::ApplyZoneParry(uint64_t sessionID, uint16_t zoneID,
        const ParryRequestBody& request, Parry parry) {
        ParryResponseBody result{request.requestID, request.ownershipEpoch, static_cast<uint8_t>(ParryStatus::Invalid), 0, 0};
        if (request.requestID == 0 || request.ownershipEpoch == 0)
            return result;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted) {
            result.status = static_cast<uint8_t>(ParryStatus::Unavailable);
            return result;
        }
        if (session->zoneID == 0 || session->zoneID != zoneID || session->ownershipEpoch != request.ownershipEpoch) {
            result.status = static_cast<uint8_t>(ParryStatus::StaleOwnership);
            return result;
        }
        return parry();
    }
}
