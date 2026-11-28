#pragma once

#include <cstdint>

namespace Core {
    // 월드와 Net/Cache에서 함께 사용하는 용량 기준.
    inline constexpr uint16_t AOICellCount = 9;
    inline constexpr uint16_t MAX_USER_CAPACITY = 7000;
    inline constexpr uint8_t MAX_CHARNAME_LEN = 32;

    template <typename T>
    constexpr bool IsPowerOfTwo(T x) {
        return x != 0 && (x & (x - 1)) == 0;
    }
}
