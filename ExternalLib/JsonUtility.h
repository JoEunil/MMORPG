#pragma once
#include <nlohmann/json.hpp>
#include <thread>

namespace Extern {
    inline void BuildJson(nlohmann::json&) {}

    template <typename Key, typename Value, typename... Rest>
    inline void BuildJson(nlohmann::json& j, Key&& key, Value&& value, Rest&&... rest) {
        j[std::forward<Key>(key)] = std::forward<Value>(value);
        BuildJson(j, std::forward<Rest>(rest)...);
    }

    inline std::string MakeLog(
        const std::string& event,
        const std::string& msg,
        nlohmann::json data = {}
    ) {
        size_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        return nlohmann::json{
            // 로그에서 now()는 부담 안됨.
            {"timestamp", ts},
            {"event", event},
            {"thread_id", thread_id},
            {"msg", msg},
            {"data", data}
        }.dump();
    }
}