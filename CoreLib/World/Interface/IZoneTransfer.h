#pragma once

#include <cstdint>
#include <memory>

namespace Core {
    struct ZoneTransfer;

    class IZoneTransfer {
    public:
        virtual ~IZoneTransfer() = default;
        virtual uint16_t FindDestination(uint16_t sourceZoneID, float x, float y) const = 0;
        virtual bool TryEnqueueTransfer(uint16_t destinationID, const std::shared_ptr<ZoneTransfer>& transfer) = 0;
        virtual bool TryEnqueueTransferRejected(const std::shared_ptr<ZoneTransfer>& transfer) = 0;
        virtual void EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) = 0;
    };
}
