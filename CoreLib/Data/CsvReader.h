#pragma once

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Data {
    class CsvReader {
        std::filesystem::path m_path;
        std::ifstream m_input;
        std::unordered_map<std::string, std::size_t> m_columns;
        std::vector<std::string> m_row;
        std::size_t m_line = 1;
        std::size_t m_rowLine = 1;
        bool m_blankRecord = false;

        bool ReadRecord(std::vector<std::string>& fields);

    public:
        CsvReader(const std::filesystem::path& path, std::initializer_list<std::string_view> columns);
        bool ReadRow();
        std::string_view Get(std::string_view column) const;
        float GetFloat(std::string_view column) const;
        [[noreturn]] void Fail(std::string_view column, std::string_view message) const;

        template<typename T>
        T GetUnsigned(std::string_view column) const {
            static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
            const auto text = Get(column);
            uint64_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || value > std::numeric_limits<T>::max())
                Fail(column, "expected an unsigned integer within the field's range");
            return static_cast<T>(value);
        }
    };
}
