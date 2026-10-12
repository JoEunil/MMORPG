#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace Base {
    class WAL;
}

namespace Core {
    enum class DurableLogType : uint16_t {
        CacheMessage = 1,
        ChatZoneTransition = 3,
    };

    class DurableLogger {
        static constexpr uint32_t SEGMENT_SIZE = 1000000;

        std::unique_ptr<Base::WAL> m_wal;

    public:
        DurableLogger();
        ~DurableLogger();

        DurableLogger(const DurableLogger&) = delete;
        DurableLogger& operator=(const DurableLogger&) = delete;

        bool Initialize(const std::string& fileName = "backpressure") noexcept;
        bool Write(DurableLogType type, const void* payload, size_t length) noexcept;
    };
}
