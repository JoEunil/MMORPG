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

        Message* Acquire(); // MessageQueue에 복사하고 바로 반납해서 수명관리가 단순함
        ScopedMessage AcquireScoped(); // The pool must outlive the returned handle.
        void Return(Message* msg);

        void InitializeForTest() {
        }
    };
}