#pragma once

#include <cstddef>
#include <cstdint>

namespace Core {
    enum class DurableLogType : uint16_t {
        CacheMessage = 1
    };

    class IDurableLog {
    public:
        virtual ~IDurableLog() = default;
        // Success includes the durable flush, not just appending to a buffer.
        virtual bool Write(DurableLogType type, const void* payload, size_t length) noexcept = 0;
    };
}
