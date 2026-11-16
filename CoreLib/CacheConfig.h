#pragma once

#include <cstddef>
#include <cstdint>

namespace Core {
    inline constexpr uint16_t MSGPOOL_SIZE = 100;
    inline constexpr uint16_t MESSAGE_LEN = 2048;
    inline constexpr uint16_t MQ_THREADPOOL_SIZE = 2; // 수신 큐
    inline constexpr std::size_t MQ_SIZE = 256;
}
