#pragma once

#include <string>
#include <memory>
#include <map>


namespace Core {
    class ILogger {
        public:
        virtual ~ILogger() = default;
        virtual void CreateSink(const std::string& logFileName) = 0;
        virtual void LogInfo(const std::string& msg) = 0;
        virtual void LogError(const std::string& msg) = 0;
        virtual void LogWarn(const std::string& msg) = 0;
    };
}
