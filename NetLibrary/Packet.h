#pragma once

#include <string>
#include <cstdint>

#include <CoreLib/PacketTypes.h>
#include <CoreLib/IPacket.h>


namespace Net {
    class Packet : public Core::IPacket {
        uint8_t* m_buffer;
        uint16_t m_length; // data length
        uint16_t m_capacity; // buffer length

    public:
        Packet(const Packet&) = delete;
        Packet& operator=(const Packet&) = delete;

        Packet(Packet&& other) noexcept
            : m_buffer(other.m_buffer), m_length(other.m_length), m_capacity(other.m_capacity) {
            other.m_buffer = nullptr;
            other.m_length = 0;
        }

        Packet& operator=(Packet&& other) noexcept {
            if (this != &other) {
                delete[] m_buffer;
                m_buffer = other.m_buffer;
                m_length = other.m_length;
                m_capacity = other.m_capacity;
                other.m_buffer = nullptr;
                other.m_length = 0;
            }
            return *this;
        }
        // m_buffer가 raw pointer라서 복사 생성 금지, 이동 생성자 정의

        Packet(uint16_t size) {
            m_buffer = new uint8_t[size];
        };
        ~Packet() {
            delete[] m_buffer;
        }

        uint8_t* GetBuffer() override { return m_buffer; }
        uint16_t GetLength() override { return m_length; }
        void SetLength(uint16_t len) { m_length = len; };
    };

    // 버퍼와 타입캐스팅으로, 직렬화, 역직렬화 기능 (메모리 복사 없음)
}
