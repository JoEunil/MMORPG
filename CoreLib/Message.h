#pragma once
#include <cstdint>

namespace Core {
    class Message {
        uint8_t* m_buffer;
        uint16_t m_length; // data length
        uint16_t m_capacity; // buffer length
    public:
        Message() = delete;
        Message(const Message&) = delete;
        Message& operator=(const Message&) = delete;
        Message(Message&&) = delete;
        Message& operator=(Message&&) = delete;

        explicit Message(uint16_t size) {
            m_buffer = new uint8_t[size];
            m_capacity = size;
            m_length = 0;
        };
        
        ~Message() {
            delete[] m_buffer;
        }

        uint8_t* GetBuffer() { return m_buffer; }
        uint16_t GetLength() const { return m_length; }
        void SetLength(uint16_t len) { m_length = len; };
    };
}
