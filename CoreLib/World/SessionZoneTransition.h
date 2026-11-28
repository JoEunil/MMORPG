#pragma once

#include <cstdint>
#include <memory>
#include <optional>

namespace Core {
    struct ZoneTransfer;
    struct LobbyEntryRequest {
        uint64_t characterID = 0;
        uint64_t ownershipEpoch = 0;
    };
    struct ZoneRequestRoute {
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
        bool alreadyPending = false;
    };
    struct RespawnRequest {
        enum class Status {
            Ignored,
            Rejected,
            Ready,
            Replay
        };

        Status status = Status::Ignored;
        uint64_t characterID = 0;
    };

    // SessionManager holds the session shard lock while accessing this object.
    class SessionZoneTransition {
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
        std::shared_ptr<ZoneTransfer> pendingTransfer;
        uint64_t lastRespawnRequestEpoch = 0;
        uint64_t bootstrapSequence = 0;

        void UpdateOwnership(uint16_t destinationID);
        bool MatchesTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t targetZoneID, bool returning) const;
        bool MatchesOwnership(uint16_t expectedZoneID, uint64_t expectedEpoch) const {
            return zoneID == expectedZoneID && ownershipEpoch == expectedEpoch;
        }

      public:
        ZoneRequestRoute GetOwnership() const { return {zoneID, ownershipEpoch}; }
        void ResetForAuth() {
            zoneID = 0;
            lastRespawnRequestEpoch = 0;
        }
        std::shared_ptr<ZoneTransfer> TakePendingTransfer();
        std::optional<uint16_t> RouteProfileVersion(uint32_t profileID, uint32_t version);
        std::optional<uint64_t> PrepareBootstrap(uint16_t expectedZoneID, uint64_t expectedEpoch, bool resendZoneChange);
        bool CompleteLobbyEntry(uint16_t destinationID, uint64_t expectedEpoch);
        std::optional<LobbyEntryRequest> GetLobbyEntry(uint64_t characterID) const;
        RespawnRequest::Status CheckRespawn(uint16_t expectedZoneID, uint64_t expectedEpoch) const;
        bool CompleteTownRespawn(uint64_t expectedEpoch);
        bool CheckAction(uint16_t expectedZoneID, uint64_t expectedEpoch, std::optional<ZoneRequestRoute>& resync);
        bool BeginTransfer(const std::shared_ptr<ZoneTransfer>& transfer);
        void CancelTransfer(const std::shared_ptr<ZoneTransfer>& transfer);
        std::optional<ZoneTransfer> ReadTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t targetZoneID, bool returning) const;
        bool CompleteTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t targetZoneID, bool returning);
        std::optional<ZoneRequestRoute> GetRespawnRoute(uint64_t expectedEpoch) const;
        std::optional<ZoneRequestRoute> GetBootstrapRoute(uint64_t expectedEpoch) const;
    };
}
