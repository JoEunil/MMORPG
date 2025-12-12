#pragma once

#include <memory>

#include "IPacketView.h"
#include "IPacketDispatcher.h"

namespace Core {
    class IPacketDispatcher {
    public:
        virtual void Process(std::shared_ptr<IPacketView> pv) = 0;
        virtual void Disconnect(uint64_t sessionID) = 0;
    };
}
