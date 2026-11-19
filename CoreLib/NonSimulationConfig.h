#pragma once

#include <cstdint>
#include "Config.h"

namespace Core {
    inline constexpr uint8_t CHANNEL_ID = 1;
    inline constexpr uint16_t NON_SIMULATION_THREADPOOL_SIZE = 3;
    inline constexpr uint16_t CHAT_QUEUE_SIZE = 512;
    inline constexpr uint16_t CHAT_DEFER_QUEUE_SIZE = 128;
    inline constexpr uint32_t NON_SIMULATION_EVENT_QUEUE_SIZE = 16384;
    inline constexpr uint32_t NON_SIMULATION_DEFER_QUEUE_SIZE = 4096;

    static_assert(IsPowerOfTwo(CHAT_QUEUE_SIZE), "CHAT_QUEUE_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(CHAT_DEFER_QUEUE_SIZE), "CHAT_DEFER_QUEUE_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(NON_SIMULATION_EVENT_QUEUE_SIZE), "NON_SIMULATION_EVENT_QUEUE_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(NON_SIMULATION_DEFER_QUEUE_SIZE), "NON_SIMULATION_DEFER_QUEUE_SIZE must be a power of two");
}
