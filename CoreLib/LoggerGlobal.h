#pragma once
#include <memory>
#include "ILogger.h"

namespace Core {
    extern std::unique_ptr<ILogger> sysLogger;
    extern std::unique_ptr<ILogger> gameLogger;
    extern std::unique_ptr<ILogger> errorLogger;
    extern std::unique_ptr<ILogger> perfLogger;
}