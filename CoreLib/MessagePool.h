#pragma once

#include <deque>
#include <mutex>
#include <cstdint>

namespace Core {
    class Message;
    class MessagePool{
        uint16_t m_remains;
        std::deque<Message*> m_messages;
        std::mutex m_mutex;

        void Initialize();
        bool IsReady() {
            return m_messages.size() > 0;
        }
        void Adjust();
        void Increase(uint16_t& size); // 풀 늘리기
        void Decrease(uint16_t& size); // 풀 줄이기
        
        friend class Initializer;
    public:
        ~MessagePool();
        Message* Acquire(); // MessageQueue에 복사하고 바로 반납해서 수명관리가 단순함
        void Return(Message* msg);
    };
}