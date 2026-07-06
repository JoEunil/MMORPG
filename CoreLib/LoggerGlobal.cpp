#include "pch.h"
#include "LoggerGlobal.h"

namespace Core {
    class NullLogger : public ILogger {
    public:
        void CreateSink(const std::string&) override {}
        void LogInfo(const std::string&) override {}
        void LogError(const std::string&) override {}
        void LogWarn(const std::string&) override {}
        void Flush() override {}
    };
    std::unique_ptr<ILogger> sysLogger = std::make_unique<NullLogger>();
    std::unique_ptr<ILogger> gameLogger = std::make_unique<NullLogger>();
    std::unique_ptr<ILogger> errorLogger = std::make_unique<NullLogger>();
    std::unique_ptr<ILogger> perfLogger = std::make_unique<NullLogger>();
}