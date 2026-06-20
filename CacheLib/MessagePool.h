#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/Message.h>
#include <BaseLib/FixedObjectPool.h>
#include "Config.h"

namespace Cache {
    class MessagePool{
		Base::FixedObjectPool<Core::Message, MSGPOOL_SIZE> m_fixedPool{ MESSAGE_LEN };
        bool IsReady() {
            return true;
        }
        
        friend class Initializer;
    public:
        Core::Message* Acquire();
        void Return(Core::Message* msg);
    };
}
