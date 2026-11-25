#pragma once
#include <cstdint>
#include "../Interface/IMessage.h"

namespace Core {
    class Message : public IMessage {
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
        
        ~Message() override {
            delete[] m_buffer;
        }

        uint8_t* GetBuffer() override { return m_buffer; }
        const uint8_t* GetBuffer() const override { return m_buffer; }
        uint16_t GetLength() const override { return m_length; }
        uint16_t GetCapacity() const override { return m_capacity; }
        void SetLength(uint16_t len) override { m_length = len; };
    };
}
