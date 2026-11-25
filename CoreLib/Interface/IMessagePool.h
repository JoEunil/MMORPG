#pragma once

#include <memory>
#include "IMessage.h"

namespace Core {
    class IMessagePool;

    struct MessageDeleter {
        IMessagePool* pool = nullptr;
        void operator()(IMessage* message) const noexcept;
    };

    using ScopedMessage = std::unique_ptr<IMessage, MessageDeleter>;

    class IMessagePool {
    public:
        virtual ~IMessagePool() = default;
        virtual ScopedMessage AcquireScoped() = 0;
        virtual void Return(IMessage* message) noexcept = 0;
    };

    inline void MessageDeleter::operator()(IMessage* message) const noexcept {
        pool->Return(message);
    }
}
