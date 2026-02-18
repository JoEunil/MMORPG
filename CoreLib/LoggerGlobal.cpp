#include "pch.h"
#include "LoggerGlobal.h"

namespace Core {
    std::unique_ptr<ILogger> sysLogger = nullptr;
    std::unique_ptr<ILogger> gameLogger = nullptr;
    std::unique_ptr<ILogger> errorLogger = nullptr;
    std::unique_ptr<ILogger> perfLogger = nullptr;
}