#pragma once

#include <cstdint>

namespace Core {
    inline constexpr uint16_t MAGIC = 0xABCD;

#pragma pack(push, 1)
    struct PacketHeader {
        uint16_t magic = MAGIC;
        uint32_t length;
        uint8_t opcode;
        uint8_t flags = 0x00;
    };
#pragma pack(pop)

}
