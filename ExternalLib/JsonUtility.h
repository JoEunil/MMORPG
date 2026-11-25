#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <functional>
#include <thread>
#include <CoreLib/Interface/ILogFormatter.h>

namespace External {
    struct JsonLogValue {
        template<typename T>
        nlohmann::json operator()(const T& value) const { return value; }
    };

    class JsonUtility final : public Core::ILogFormatter {
    public:
        std::string Format(const Core::LogRecord& record) const override {
            nlohmann::json data;
            for (const auto& field : record.fields)
                data[field.key] = std::visit(JsonLogValue{}, field.value);
            size_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            return nlohmann::json{
                {"timestamp", ts},
                {"event", std::string(record.event)},
                {"thread_id", thread_id},
                {"msg", std::string(record.message)},
                {"data", data}
            }.dump();
        }
    };
}
