#include "MessagePool.h"
#include "Config.h"

#include <CoreLib/Message.h>

#include <memory>
#include <mutex>

namespace Cache {
    void MessagePool::Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i < TARGET_MSGPOOL_SIZE; i++)
        {
            Core::Message* message = new Core::Message(MESSGAGE_LEN);
            m_messages.push_back(message);
        };
    }
    MessagePool::~MessagePool() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (Core::Message* msg : m_messages) {
            delete msg;
        }
        m_messages.clear();
    }

    void MessagePool::Adjust()
    {
        uint16_t current = m_messages.size();

        if (current > MAX_MSGPOOL_SIZE) {
            Decrease(current);
        }
        if (current < MIN_MSGPOOL_SIZE) {
            Increase(current);
        }
    }

    void MessagePool::Increase(uint16_t& size) {
        auto current = m_messages.size();

        if (current < MIN_MSGPOOL_SIZE) {
            while (current < TARGET_MSGPOOL_SIZE)
            {
                Core::Message* message = new Core::Message(MESSGAGE_LEN);
                m_messages.push_back(message);
                current++;
            }
        }
    }

    void MessagePool::Decrease(uint16_t& size) {
        auto current = m_messages.size();

        if (current > MAX_MSGPOOL_SIZE) {
            while (current > TARGET_MSGPOOL_SIZE)
            {
                Core::Message* temp = m_messages.front();
                m_messages.pop_front();
                delete temp;
                current--;
            }
        }
    }

    Core::Message* MessagePool::Acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_messages.empty()) {
            return nullptr;
        }
        Core::Message* msg = m_messages.back();
        m_messages.pop_back();
        Adjust();
        return msg;
    }

    void MessagePool::Return(Core::Message* msg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push_back(msg);
        Adjust();
    }

}
