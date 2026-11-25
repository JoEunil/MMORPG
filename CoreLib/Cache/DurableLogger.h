#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include "../Interface/IDurableLog.h"

namespace Base {
    class WAL;
}

namespace Core {
    class DurableLogger : public IDurableLog {
        static constexpr uint32_t SEGMENT_SIZE = 1000000;

        std::unique_ptr<Base::WAL> m_wal;

    public:
        DurableLogger();
        ~DurableLogger() override;

        DurableLogger(const DurableLogger&) = delete;
        DurableLogger& operator=(const DurableLogger&) = delete;

        bool Initialize(const std::string& fileName = "backpressure") noexcept;
        bool Write(DurableLogType type, const void* payload, size_t length) noexcept override;
    };
}
