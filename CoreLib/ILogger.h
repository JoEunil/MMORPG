#pragma once

#include <string>
#include <External/nlohmann/json.hpp>
#include <ExternalLib/JsonUtility.h>
namespace Core {
    class ILogger {
    private:
    public:
        virtual ~ILogger() = default;
        virtual void CreateSink(const std::string& logFileName) = 0;
        virtual void LogInfo(const std::string& msg) = 0;
        virtual void LogError(const std::string& msg) = 0;
        virtual void LogWarn(const std::string& msg) = 0;
        virtual void Flush() = 0;

        template <typename... Args>
        void LogInfo(const std::string& event, const std::string& msg, Args&&... args) {
            nlohmann::json j;
            Extern::BuildJson(j, std::forward<Args>(args)...);
            this->LogInfo(Extern::MakeLog(event, msg, j));
        }

        template <typename... Args>
        void LogError(const std::string& event, const std::string& msg, Args&&... args) {
            nlohmann::json j;
            Extern::BuildJson(j, std::forward<Args>(args)...);
            this->LogError(Extern::MakeLog(event, msg, j));
        }

        template <typename... Args>
        void LogWarn(const std::string& event, const std::string& msg, Args&&... args) {
            nlohmann::json j;
            Extern::BuildJson(j, std::forward<Args>(args)...);
            this->LogWarn(Extern::MakeLog(event, msg, j));
        }

    };
}
