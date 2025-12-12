#pragma once
#include <cstdint>

namespace Core {
    class Message {
        uint8_t* m_buffer;
        uint16_t m_length; // data length
        uint16_t m_capacity; // buffer length
    public:
        Message() = delete;
        // m_buffer가 raw pointer라서 복사 생성 금지, 이동 생성자 정의

        Message(uint16_t size) {
            m_buffer = new uint8_t[size];
            m_capacity = size;
            m_length = 0;
        };
        
        ~Message() {
            delete[] m_buffer;
        }

        uint8_t* GetBuffer() { return m_buffer; }
        uint16_t GetLength() { return m_length; }
        void SetLength(uint16_t len) { m_length = len; };
    };
}
