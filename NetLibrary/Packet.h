#pragma once

#include <string>
#include <cstdint>

#include <CoreLib/PacketTypes.h>
#include <CoreLib/IPacket.h>
#include <CoreLib/IPacketPool.h>

namespace Net {
    class Packet : public Core::IPacket {
        uint8_t* m_buffer;
        uint32_t m_length; 
        uint32_t m_capacity;
        Core::IPacketPool* owner;

    public:
        Packet(const Packet&) = delete;
        Packet& operator=(const Packet&) = delete;

        Packet(Packet&&) = delete;
        Packet& operator=(Packet&&) = delete;

        Packet(uint32_t size, Core::IPacketPool* o) {
            m_buffer = new uint8_t[size];
            m_length = 0;
            m_capacity = size;
            owner = o;
        };
        ~Packet() {
            delete[] m_buffer;
        }

        void Release() override;
        uint8_t* GetBuffer() override { return m_buffer; }
        uint32_t GetLength() const override { return m_length; }
        uint32_t GetCapacity() const override { return m_capacity; }
        void SetLength(uint32_t len) override { m_length = len; }
    };
}
