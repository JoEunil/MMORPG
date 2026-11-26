#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <CoreLib/Config.h>
#include <CoreLib/Packet/PacketTypes.h>

namespace Net {
    // Net에서 사용하는 Core 계약(타입·상수·프로토콜 규칙·용량)을 모아 연결하는 곳.
    using PacketHeader = Core::PacketHeader;
    using Pong = Core::Pong;

    static_assert(sizeof(PacketHeader) == 8);
    static_assert(offsetof(PacketHeader, length) == 2);
    static_assert(offsetof(PacketHeader, opcode) == 6);
    static_assert(offsetof(PacketHeader, flags) == 7);

    inline constexpr uint16_t MAGIC = Core::MAGIC;
    inline constexpr uint16_t MAX_DEFINED_OPCODE = Core::MAX_DEFINED_OPCODE;
    inline constexpr uint8_t AUTH_OPCODE = static_cast<uint8_t>(Core::OP::AUTH);
    inline constexpr uint8_t PONG_OPCODE = static_cast<uint8_t>(Core::OP::PONG);
    inline constexpr uint8_t MASK_EXIST = Core::MASK_EXIST;
    inline constexpr uint8_t MASK_AUTHENTICATED = Core::MASK_AUTHENTICATED;
    inline constexpr uint8_t MASK_NOT_CHEAT = Core::MASK_NOT_CHEAT;

    // 프로토콜 규칙의 구현은 Core에 두고, 수신 필터에서 사용할 함수를 연결한다.
    inline constexpr auto GetReqPacketLen = &Core::GetReqPacketLen;
    inline constexpr auto IsElasticReqPacket = &Core::IsElasticReqPacket;
    inline constexpr auto IsSimulationFlag = &Core::IsSimulationFlag;
    inline constexpr auto IsSimulationOpcode = &Core::IsSimulationOpcode;

    inline constexpr size_t PACKET_HEADER_SIZE = sizeof(Core::PacketHeader);
    inline constexpr uint32_t MAX_REQ_PACKET_LEN = Core::MAX_REQ_PACKET_LEN;
    inline constexpr uint16_t SEND_BUFFER_RESERVE = Core::AOICellCount;
    inline constexpr size_t REQUIRED_GIANT_PACKET_LENGTH = (std::max)({
        sizeof(Core::PacketStruct<Core::FullSnapshotBody>),
        sizeof(Core::PacketStruct<Core::MonsterFullSnapshotBody>),
        Core::MAX_ZONE_CAPACITY * 10 * sizeof(Core::DeltaUpdateField)
    });
}
