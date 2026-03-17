#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

#include <CoreLib/LoggerGlobal.h>
#include "Config.h"

namespace Core {
    class Message;
}
namespace Cache {
    class MessagePool{
        uint16_t m_remains;
        std::vector<Core::Message*> m_messages;
        std::mutex m_mutex;

        void Initialize();
        bool IsReady() {
            if (m_messages.size() > MIN_MSGPOOL_SIZE) {
                Core::sysLogger->LogError("cache message pool", "invalid pool size");
                return false;
            }
            return true;
        }
        void Adjust();
        void Increase(); 
        void Decrease();
        ~MessagePool();
        
        friend class Initializer;
    public:
        Core::Message* Acquire();
        void Return(Core::Message* msg);
    };
}
