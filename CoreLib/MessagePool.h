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
                pool->Return(msg);
            }
        };

        bool IsReady() {
            return true;
        }

        friend class Initializer;
    public:
        using ScopedMessage = std::unique_ptr<Message, MessageDeleter>;

        Message* Acquire() {
            return m_fixedPool.Allocate();
        }

        ScopedMessage AcquireScoped() {
            return ScopedMessage{ Acquire(), MessageDeleter{ this } };
        }

        void Return(Message* msg) {
            m_fixedPool.Deallocate(msg);
        }
    };

    using CoreMessagePool = MessagePool<MSGPOOL_SIZE, MESSAGE_LEN>;
}
