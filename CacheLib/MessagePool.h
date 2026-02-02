#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

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
            return m_messages.size() > 0;
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
