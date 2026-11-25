#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <CoreLib/Config.h>
#include <CoreLib/Packet/PacketTypes.h>

namespace Net {
    // Core의 프로토콜·월드 정의를 Net에서 사용할 설정값으로 연결한다.
    inline constexpr size_t PACKET_HEADER_SIZE = sizeof(Core::PacketHeader);
    inline constexpr uint32_t MAX_REQ_PACKET_LEN = Core::MAX_REQ_PACKET_LEN;
    inline constexpr uint16_t SEND_BUFFER_RESERVE = Core::AOICellCount;
    inline constexpr size_t REQUIRED_GIANT_PACKET_LENGTH = (std::max)({
        sizeof(Core::PacketStruct<Core::FullSnapshotBody>),
        sizeof(Core::PacketStruct<Core::MonsterFullSnapshotBody>),
        Core::MAX_ZONE_CAPACITY * 10 * sizeof(Core::DeltaUpdateField)
    });
}
