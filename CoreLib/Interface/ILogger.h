#pragma once

#include <initializer_list>
#include <string>
#include "LogRecord.h"
namespace Core {
    class ILogger {
    public:
        virtual ~ILogger() = default;
        virtual void CreateSink(const std::string& logFileName) = 0;
        virtual void LogInfo(const std::string& msg) = 0;
        virtual void LogError(const std::string& msg) = 0;
        virtual void LogWarn(const std::string& msg) = 0;
        virtual void Flush() = 0;

        virtual void LogInfo(const std::string& event, const std::string& msg,
            std::initializer_list<LogField> fields = {}) = 0;
        virtual void LogError(const std::string& event, const std::string& msg,
            std::initializer_list<LogField> fields = {}) = 0;
        virtual void LogWarn(const std::string& event, const std::string& msg,
            std::initializer_list<LogField> fields = {}) = 0;

    };
}
