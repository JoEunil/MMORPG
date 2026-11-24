#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Core {
    enum class LogLevel { Info, Warning, Error };

    using LogValue = std::variant<bool, int64_t, uint64_t, double, std::string, std::vector<std::byte>>;

    struct LogField {
        std::string key;
        LogValue value;

        template<typename T>
        LogField(std::string_view name, T&& data)
            : key(name), value(ToValue(std::forward<T>(data))) {}

    private:
        template<typename T>
        static LogValue ToValue(T&& data) {
            using Value = std::remove_cvref_t<T>;
            if constexpr (std::is_enum_v<Value>)
                return ToValue(static_cast<std::underlying_type_t<Value>>(data));
            else if constexpr (std::is_same_v<Value, bool>)
                return data;
            else if constexpr (std::is_integral_v<Value> && std::is_signed_v<Value>)
                return static_cast<int64_t>(data);
            else if constexpr (std::is_integral_v<Value>)
                return static_cast<uint64_t>(data);
            else if constexpr (std::is_floating_point_v<Value>)
                return static_cast<double>(data);
            else if constexpr (std::is_convertible_v<T, std::string_view>)
                return std::string(std::string_view(data));
            else
                return LogValue{std::forward<T>(data)};
        }
    };

    struct LogRecord {
        LogLevel level;
        std::string_view event;
        std::string_view message;
        std::span<const LogField> fields; // 호출한 스레드에서 json 만들고 비동기 처리되기 때문에 안전
    };
}
