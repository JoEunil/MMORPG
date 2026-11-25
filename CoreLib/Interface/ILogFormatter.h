#pragma once

#include <string>
#include "LogRecord.h"

namespace Core {
    class ILogFormatter {
    public:
        virtual ~ILogFormatter() = default;
        virtual std::string Format(const LogRecord& record) const = 0;
    };
}
