#pragma once

#include <cstddef>
#include <memory>

#include <BaseLib/FixedObjectPool.h>
#include "Config.h"
#include "Message.h"

namespace Core {
    template<std::size_t PoolSize, uint16_t MessageLength>
    class MessagePool {
        Base::FixedObjectPool<Message, PoolSize> m_fixedPool{ MessageLength };

        struct MessageDeleter {
            MessagePool* pool;

            void operator()(Message* msg) const noexcept {
                pool->m_fixedPool.Deallocate(msg);
            }
        };

        bool IsReady() {
            return true;
        }

        friend class Initializer;
    public:
        using ScopedMessage = std::unique_ptr<Message, MessageDeleter>;

        ScopedMessage AcquireScoped() {
            return ScopedMessage{ m_fixedPool.Allocate(), MessageDeleter{ this } };
        }

    };

    using CoreMessagePool = MessagePool<MSGPOOL_SIZE, MESSAGE_LEN>;
}
