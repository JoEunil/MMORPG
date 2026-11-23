#pragma once

#include "IResultReader.h"

namespace sql {
    class ResultSet;
}

namespace Cache {
    class MySQLResultAdapter final : public IResultReader {
        std::unique_ptr<sql::ResultSet> result_;

    public:
        explicit MySQLResultAdapter(std::unique_ptr<sql::ResultSet> result);
        ~MySQLResultAdapter() override;

        bool next() override;
        bool isNull(std::string_view column) const override;
        int32_t getInt(std::string_view column) const override;
        uint32_t getUInt(std::string_view column) const override;
        int64_t getInt64(std::string_view column) const override;
        uint64_t getUInt64(std::string_view column) const override;
        long double getDouble(std::string_view column) const override;
        std::string getString(std::string_view column) const override;
        std::unique_ptr<std::istream> getBlob(std::string_view column) const override;
    };
}
