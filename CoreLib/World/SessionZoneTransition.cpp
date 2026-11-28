#include "pch.h"
#include "SessionZoneTransition.h"
#include "ZoneState.h"
#include <limits>
#include <utility>

namespace Core {
    void SessionZoneTransition::UpdateOwnership(uint16_t destinationID) {
        zoneID = destinationID;
        ++ownershipEpoch;
    }

    bool SessionZoneTransition::MatchesTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t targetZoneID, bool returning) const {
        return transfer && pendingTransfer == transfer && MatchesOwnership(transfer->sourceZoneID, transfer->ownershipEpoch) &&
            returning == (targetZoneID == transfer->sourceZoneID);
    }

    std::shared_ptr<ZoneTransfer> SessionZoneTransition::TakePendingTransfer() {
        return std::exchange(pendingTransfer, {});
    }

    std::optional<uint16_t> SessionZoneTransition::RouteProfileVersion(uint32_t profileID, uint32_t version) {
        if (pendingTransfer) {
            pendingTransfer->character.UpdateProfileVersion(profileID, version);
            return std::nullopt;
        }
        return zoneID;
    }

    std::optional<uint64_t> SessionZoneTransition::PrepareBootstrap(uint16_t expectedZoneID, uint64_t expectedEpoch, bool resendZoneChange) {
        if (!MatchesOwnership(expectedZoneID, expectedEpoch) || pendingTransfer)
            return std::nullopt;
        if (resendZoneChange)
            return 0;
        if (bootstrapSequence == (std::numeric_limits<uint64_t>::max)())
            return std::nullopt;
        return ++bootstrapSequence;
    }

    bool SessionZoneTransition::CompleteLobbyEntry(uint16_t destinationID, uint64_t expectedEpoch) {
        if (zoneID != 0 || ownershipEpoch != expectedEpoch)
            return false;
        UpdateOwnership(destinationID);
        return true;
    }

    std::optional<LobbyEntryRequest> SessionZoneTransition::GetLobbyEntry(uint64_t characterID) const {
        if (zoneID != 0 || ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return std::nullopt;
        return LobbyEntryRequest{characterID, ownershipEpoch};
    }

    RespawnRequest::Status SessionZoneTransition::CheckRespawn(uint16_t expectedZoneID, uint64_t expectedEpoch) const {
        if (zoneID != expectedZoneID)
            return RespawnRequest::Status::Ignored;
        if (pendingTransfer && pendingTransfer->IsRespawn() && ownershipEpoch == expectedEpoch)
            return RespawnRequest::Status::Ignored;
        if (pendingTransfer || expectedEpoch == (std::numeric_limits<uint64_t>::max)())
            return RespawnRequest::Status::Rejected;
        if (ownershipEpoch == expectedEpoch)
            return RespawnRequest::Status::Ready;
        if (zoneID == TOWN_ZONE_ID && lastRespawnRequestEpoch == expectedEpoch && ownershipEpoch == expectedEpoch + 1)
            return RespawnRequest::Status::Replay;
        return RespawnRequest::Status::Rejected;
    }

    bool SessionZoneTransition::CompleteTownRespawn(uint64_t expectedEpoch) {
        if (zoneID != TOWN_ZONE_ID || pendingTransfer || expectedEpoch == (std::numeric_limits<uint64_t>::max)())
            return false;
        if (ownershipEpoch == expectedEpoch) {
            lastRespawnRequestEpoch = expectedEpoch;
            UpdateOwnership(TOWN_ZONE_ID);
        } else if (lastRespawnRequestEpoch != expectedEpoch || ownershipEpoch != expectedEpoch + 1) {
            return false;
        }
        return true;
    }

    bool SessionZoneTransition::CheckAction(uint16_t expectedZoneID, uint64_t expectedEpoch, std::optional<ZoneRequestRoute>& resync) {
        resync.reset();
        if (zoneID == 0 || ownershipEpoch == 0 || pendingTransfer)
            return false;
        if (!MatchesOwnership(expectedZoneID, expectedEpoch)) {
            resync = ZoneRequestRoute{zoneID, ownershipEpoch};
            return false;
        }
        lastRespawnRequestEpoch = 0;
        return true;
    }

    bool SessionZoneTransition::BeginTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer || zoneID == 0 || !MatchesOwnership(transfer->sourceZoneID, transfer->ownershipEpoch) || pendingTransfer)
            return false;
        pendingTransfer = transfer;
        return true;
    }

    void SessionZoneTransition::CancelTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (pendingTransfer == transfer)
            pendingTransfer.reset();
    }

    std::optional<ZoneTransfer> SessionZoneTransition::ReadTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t targetZoneID, bool returning) const {
        if (!MatchesTransfer(transfer, targetZoneID, returning))
            return std::nullopt;
        // Profile updates can change the pending record; admission uses a private copy.
        return *transfer;
    }

    bool SessionZoneTransition::CompleteTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t targetZoneID, bool returning) {
        if (!MatchesTransfer(transfer, targetZoneID, returning))
            return false;
        pendingTransfer.reset();
        if (!returning) {
            if (targetZoneID != 0 && transfer->IsRespawn())
                lastRespawnRequestEpoch = transfer->ownershipEpoch;
            UpdateOwnership(targetZoneID);
        }
        return true;
    }

    std::optional<ZoneRequestRoute> SessionZoneTransition::GetRespawnRoute(uint64_t expectedEpoch) const {
        if (zoneID == 0 || zoneID > ZONE_COUNT || expectedEpoch == (std::numeric_limits<uint64_t>::max)())
            return std::nullopt;
        if (pendingTransfer) {
            if (!pendingTransfer->IsRespawn() || expectedEpoch != ownershipEpoch)
                return std::nullopt;
            return ZoneRequestRoute{zoneID, expectedEpoch, true};
        }
        const bool replay = zoneID == TOWN_ZONE_ID && lastRespawnRequestEpoch == expectedEpoch && ownershipEpoch == expectedEpoch + 1;
        if (ownershipEpoch != expectedEpoch && !replay)
            return std::nullopt;
        return ZoneRequestRoute{zoneID, expectedEpoch};
    }

    std::optional<ZoneRequestRoute> SessionZoneTransition::GetBootstrapRoute(uint64_t expectedEpoch) const {
        if (zoneID == 0 || ownershipEpoch == 0 || pendingTransfer || ownershipEpoch != expectedEpoch)
            return std::nullopt;
        return ZoneRequestRoute{zoneID, ownershipEpoch};
    }
}
