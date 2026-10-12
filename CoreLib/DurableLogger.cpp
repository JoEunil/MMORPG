#include "pch.h"
#include "DurableLogger.h"

#include <BaseLib/WAL.h>

#include "LoggerGlobal.h"

namespace Core {
    DurableLogger::DurableLogger() = default;
    DurableLogger::~DurableLogger() = default;

    bool DurableLogger::Initialize(const std::string& fileName) noexcept {
        try {
            m_wal = std::make_unique<Base::WAL>(fileName, SEGMENT_SIZE, [](const Base::WALHeader&, const uint8_t*) {});
            if (m_wal->ReplayDegraded()) {
                errorLogger->LogError("durable logger", "closed durable log segment corrupted - quarantined, resuming on a fresh segment");
            }
            return true;
        }
        catch (const std::exception& e) {
            errorLogger->LogError("durable logger", "initialization failed", "error", e.what());
        }
        catch (...) {
            errorLogger->LogError("durable logger", "initialization failed", "error", "unknown exception");
        }
        return false;
    }

    bool DurableLogger::Write(DurableLogType type, const void* payload, size_t length) noexcept {
        if (!m_wal) {
            errorLogger->LogError("durable logger", "write requested before initialization", "type", static_cast<uint16_t>(type));
            return false;
        }
        if (payload == nullptr || length == 0) {
            errorLogger->LogError("durable logger", "invalid payload", "type", static_cast<uint16_t>(type), "length", length);
            return false;
        }

        try {
            const auto lsn = m_wal->Write(static_cast<const uint8_t*>(payload), length, static_cast<uint16_t>(type));
            if (lsn == 0) {
                errorLogger->LogError("durable logger", "WAL append failed", "type", static_cast<uint16_t>(type), "length", length);
                return false;
            }
            if (!m_wal->Fsync()) {
                errorLogger->LogError("durable logger", "WAL fsync failed", "type", static_cast<uint16_t>(type), "lsn", lsn);
                return false;
            }
            return true;
        }
        catch (const std::exception& e) {
            errorLogger->LogError("durable logger", "write failed", "type", static_cast<uint16_t>(type), "error", e.what());
        }
        catch (...) {
            errorLogger->LogError("durable logger", "write failed", "type", static_cast<uint16_t>(type), "error", "unknown exception");
        }
        return false;
    }
}
