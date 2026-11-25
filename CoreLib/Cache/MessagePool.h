#pragma once

#include <cstddef>
#include <memory>

#include <BaseLib/FixedObjectPool.h>
#include "CacheConfig.h"
#include "Message.h"
#include "../Interface/IMessagePool.h"

namespace Core {
    template<std::size_t PoolSize, uint16_t MessageLength>
    class MessagePool : public IMessagePool {
        Base::FixedObjectPool<Message, PoolSize> m_fixedPool{ MessageLength };

        bool IsReady() {
            return true;
        }

        friend class Initializer;
    public:
        using ScopedMessage = Core::ScopedMessage;

        ScopedMessage AcquireScoped() override {
            return ScopedMessage{ m_fixedPool.Allocate(), MessageDeleter{ this } };
        }

        void Return(IMessage* message) noexcept override {
            m_fixedPool.Deallocate(static_cast<Message*>(message));
        }

    };

    using CoreMessagePool = MessagePool<MSGPOOL_SIZE, MESSAGE_LEN>;
}
