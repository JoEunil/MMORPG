#pragma once

#include <deque>
#include <cstdint>

#include <BaseLib/FixedObjectPool.h>
#include "Config.h"
#include "LoggerGlobal.h"
#include "Message.h"

namespace Core {
    class MessagePool{
        Base::FixedObjectPool<Message, MSGPOOL_SIZE> m_fixedPool{ MESSGAGE_LEN };

        bool IsReady() {
            return true;
        }

        friend class Initializer;
    public:
        Message* Acquire(); // MessageQueue에 복사하고 바로 반납해서 수명관리가 단순함
        void Return(Message* msg);

    #ifdef TEST_BAZAAR
        void InitializeForTest() {
        }
    #endif
    };
}