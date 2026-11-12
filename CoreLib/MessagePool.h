#pragma once

#include <deque>
#include <cstdint>
#include <memory>

#include <BaseLib/FixedObjectPool.h>
#include "Config.h"
#include "LoggerGlobal.h"
#include "Message.h"

namespace Core {
    class MessagePool{
        Base::FixedObjectPool<Message, MSGPOOL_SIZE> m_fixedPool{ MESSAGE_LEN };

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

        Message* Acquire(); 
        ScopedMessage AcquireScoped();
        void Return(Message* msg);
    };
}