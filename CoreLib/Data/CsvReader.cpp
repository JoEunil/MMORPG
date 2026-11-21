#include "pch.h"
#include "CsvReader.h"
#include <cmath>

namespace Data {
    CsvReader::CsvReader(const std::filesystem::path& path, std::initializer_list<std::string_view> columns)
        : m_path(path), m_input(path, std::ios::binary) {
        if (!m_input)
            Fail("", "cannot open CSV file");

        char bom[3]{};
        m_input.read(bom, sizeof(bom));
        if (m_input.gcount() != 3 || std::string_view(bom, 3) != "\xEF\xBB\xBF") {
            m_input.clear();
            m_input.seekg(0);
        }

        std::vector<std::string> header;
        if (!ReadRecord(header))
            Fail("", "missing header");
        for (std::size_t index = 0; index < header.size(); ++index) {
            if (header[index].empty() || !m_columns.emplace(header[index], index).second)
                Fail(header[index], "empty or duplicate column name");
        }
        for (const auto column : columns)
            if (!m_columns.contains(std::string(column)))
                Fail(column, "missing required column");
        if (m_columns.size() != columns.size())
            Fail("", "unexpected column in header");
    }

    bool CsvReader::ReadRecord(std::vector<std::string>& fields) {
        enum class State { Unquoted, Quoted, AfterQuote };
        State state = State::Unquoted;
        fields.clear();
        std::string field;
        bool consumed = false;
        m_rowLine = m_line;
        m_blankRecord = false;
        for (;;) {
            const int next = m_input.get();
            if (next == std::char_traits<char>::eof()) {
                if (m_input.bad())
                    Fail("", "CSV read failed");
                if (state == State::Quoted)
                    Fail("", "unterminated quoted field");
                if (!consumed)
                    return false;
                fields.push_back(std::move(field));
                return true;
            }
            const bool atRecordStart = !consumed;
            consumed = true;
            const char character = static_cast<char>(next);
            if (state == State::Quoted) {
                if (character == '"') {
                    if (m_input.peek() == '"') {
                        m_input.get();
                        field.push_back('"');
                    }
                    else {
                        state = State::AfterQuote;
                    }
                }
                else {
                    field.push_back(character);
                    if (character == '\n' || (character == '\r' && m_input.peek() != '\n'))
                        ++m_line;
                }
                continue;
            }
            if (character == ',') {
                fields.push_back(std::move(field));
                field.clear();
                state = State::Unquoted;
            }
            else if (character == '\r' || character == '\n') {
                m_blankRecord = atRecordStart;
                if (character == '\r' && m_input.peek() == '\n')
                    m_input.get();
                ++m_line;
                fields.push_back(std::move(field));
                return true;
            }
            else if (character == '"' && state == State::Unquoted && field.empty()) {
                state = State::Quoted;
            }
            else {
                if (state == State::AfterQuote || character == '"')
                    Fail("", "invalid quote in CSV field");
                field.push_back(character);
            }
        }
    }

    bool CsvReader::ReadRow() {
        while (ReadRecord(m_row)) {
            if (m_blankRecord)
                continue;
            if (m_row.size() != m_columns.size())
                Fail("", "field count does not match header");
            return true;
        }
        return false;
    }

    std::string_view CsvReader::Get(std::string_view column) const {
        const auto found = m_columns.find(std::string(column));
        if (found == m_columns.end())
            Fail(column, "unknown column");
        return m_row.at(found->second);
    }

    float CsvReader::GetFloat(std::string_view column) const {
        const auto text = Get(column);
        float value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value))
            Fail(column, "expected a finite floating-point value within the field's range");
        return value;
    }

    void CsvReader::Fail(std::string_view column, std::string_view message) const {
        throw std::runtime_error(m_path.string() + ":" + std::to_string(m_rowLine) +
            (column.empty() ? "" : " [" + std::string(column) + "]") + ": " + std::string(message));
    }
}
