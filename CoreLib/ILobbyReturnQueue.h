#pragma once

#include <memory>

namespace Core {
    struct FieldZoneTransfer;

    class ILobbyReturnQueue {
    public:
        virtual ~ILobbyReturnQueue() = default;
        virtual void EnqueueLobbyReturn(const std::shared_ptr<FieldZoneTransfer>& transfer) = 0;
    };
}
