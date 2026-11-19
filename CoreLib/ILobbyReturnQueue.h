#pragma once

#include <memory>

namespace Core {
    struct ZoneTransfer;

    class ILobbyReturnQueue {
    public:
        virtual ~ILobbyReturnQueue() = default;
        virtual void EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) = 0;
    };
}
