#pragma once

#include <vector>
#include <mutex>
#include <shared_mutex>
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
#include "ZoneSessionContext.h"
#include "SessionZoneTransition.h"
#include "../Packet/PacketConfig.h"
#include "../Interface/IPacket.h"
#include "Interface/ISessionStatusReader.h"
#include "Interface/IZoneRequestQueue.h"
#include "../Packet/PacketTypes.h"

namespace Core {
    struct SessionData {
        SessionZoneTransition zoneTransition;
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
    class ISessionLifecycleListener;
    struct ZoneTransfer;
    struct ProfileVersionUpdate;
    struct ZoneChangeResult;
    enum class SessionRequestStatus {
        Ready,
        Invalid,
        StaleOwnership,
        StaleRequest,
        Replay
    };
    enum class ZoneOperationResult {
        NoResponse,
        Rejected,
        Completed
    };
    struct DisconnectRequest {
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
        uint64_t chatID = 0;
    };
    struct ZoneTransferCommit {
        uint64_t chatID = 0;
        uint64_t ownershipEpoch = 0;
        uint32_t profileID = 0;
        uint32_t profileVersion = 0;
    };
    struct ZoneBootstrapInfo {
        uint64_t chatID = 0;
        uint64_t bootstrapID = 0;
    };
    class SessionManager : public ISessionStatusReader {
        std::array<SessionShard, SHARD_SIZE> m_shards; // mutex있어서 vector 사용 불가
        std::atomic<bool> m_running;

        ISessionLifecycleListener* lifecycleListener = nullptr;

        void Initialize(ISessionLifecycleListener& listener) {
            for (auto& shard : m_shards)
                shard.sessionMap.reserve(MAX_USER_CAPACITY / SHARD_SIZE);
            lifecycleListener = &listener;
            m_running.store(true, std::memory_order_relaxed);
        }

        bool IsReady() const {

            if (lifecycleListener == nullptr) {
                sysLogger->LogError("core session manager", "session lifecycle listener not initialized");
                return false;
            }
            return true;
        }

        std::vector<std::shared_ptr<ZoneTransfer>> Shutdown();
        static SessionData* FindSessionLocked(SessionShard& shard, uint64_t sessionID) {
            const auto found = shard.sessionMap.find(sessionID);
            return found != shard.sessionMap.end() ? &found->second : nullptr;
        }

        friend class Initializer;
        friend class ZoneBenchmarkAccess;

      public:
        ~SessionManager() override { Shutdown(); }

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
            it->second.zoneTransition.ResetForAuth();
            it->second.characterID = 0;
            it->second.cheatCount = 0;
            it->second.lastCheatTime = std::chrono::steady_clock::now();
            it->second.authenticated = authenticated;
            it->second.authCompleted = true;
            Core::gameLogger->LogInfo("core session manager", "AddSession", {{"session", sessionID}, {"userID", userID}});
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
                return;
            it->second.characterID = characterID;
            Core::gameLogger->LogInfo("core session manager", "SetCharacterID", {{"session", sessionID}, {"characterID", characterID}});
        }

        uint64_t GetCharacterID(uint64_t sessionID) const {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);

            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return 0;
            return it->second.characterID;
        }

        int16_t GetZoneID(uint64_t sessionID) const override {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end()) {
                return -1;
            }
            return it->second.zoneTransition.GetOwnership().zoneID;
        }

        uint64_t GetFieldCharacterID(uint64_t sessionID) const {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            const auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end() || !it->second.authCompleted || !it->second.authenticated || it->second.userID == 0 ||
                it->second.characterID == INVALID_CHARACTER_ID || it->second.zoneTransition.GetOwnership().zoneID == 0 ||
                it->second.zoneTransition.GetOwnership().zoneID > ZONE_COUNT)
                return INVALID_CHARACTER_ID;
            return it->second.characterID;
        }

        bool IsFieldSession(uint64_t sessionID) const { return GetFieldCharacterID(sessionID) != INVALID_CHARACTER_ID; }

        uint8_t HealthCheck(uint64_t sessionID) const override {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            uint8_t res = 0;
            if (it == shard.sessionMap.end()) {
                return res;
            }

            auto status = it->second;
            lock.unlock();

            res |= MASK_EXIST;
            if (status.authenticated) {
                res |= MASK_AUTHENTICATED;
            } else {
                errorLogger->LogError("core session manager", "not authenticated", {{"sessionID", sessionID}});
            }
            if (status.cheatCount <= MAX_CHEAT_COUNT) {
                res |= MASK_NOT_CHEAT;
            } else {
                errorLogger->LogError("core session manager", "cheat detected", {{"sessionID", sessionID}, {"cheatCount", status.cheatCount}});
            }
            return res;
        }
        SessionRequestStatus CheckEquipment(uint64_t sessionID, uint16_t zoneID, uint64_t requestID, uint64_t ownershipEpoch, bool write, uint8_t slot,
            uint32_t itemID, EquipmentResponseBody& cachedResponse) const;
        void RememberEquipment(uint64_t sessionID, uint16_t zoneID, uint32_t itemID, const EquipmentResponseBody& result);
        std::optional<DisconnectRequest> BeginDisconnect(uint64_t sessionID);
        bool CompleteLobbyDisconnect(uint64_t sessionID, uint64_t ownershipEpoch);
        bool CompleteZoneDisconnect(const ZoneSessionContext& context, std::shared_ptr<ZoneTransfer>& pending);
        std::optional<LobbyEntryRequest> GetLobbyEntry(uint64_t sessionID, uint8_t op) const;
        std::optional<ZoneRequestRoute> GetRespawnRoute(uint64_t sessionID, uint64_t ownershipEpoch) const;
        std::optional<ZoneRequestRoute> GetBootstrapRoute(uint64_t sessionID, uint64_t ownershipEpoch) const;

        bool CheckZoneAction(const ZoneSessionContext& context, std::optional<ZoneRequestRoute>& resync);
        bool BeginZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer);
        void CancelZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer);
        RespawnRequest CheckRespawnRequest(const ZoneSessionContext& context) const;
        bool CompleteTownRespawn(uint64_t sessionID, uint64_t ownershipEpoch, ZoneChangeResult& result);
        std::optional<ZoneTransfer> ReadZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t zoneID, bool returning) const;
        std::optional<ZoneTransferCommit> CompleteZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t zoneID, bool returning);
        bool CompleteLobbyLoad(uint64_t sessionID, uint64_t characterID);
        bool CompleteLobbyEntry(const ZoneSessionContext& context, uint32_t profileID, ZoneChangeResult& result);

        std::optional<uint16_t> RouteProfileVersion(const ProfileVersionUpdate& update);
        std::optional<ZoneBootstrapInfo> PrepareZoneBootstrap(const ZoneSessionContext& context, bool resendZoneChange);
        std::optional<ParryStatus> GetParryRejection(uint64_t sessionID, uint16_t zoneID, const ParryRequestBody& request) const;

        void Cheat(uint64_t sessionID, uint16_t cheat, std::chrono::steady_clock::time_point time) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it != shard.sessionMap.end()) {
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
