#pragma once

#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <string_view>

namespace Cache {
    // Consume the reader before its connection/statement is reused or returned to the pool.
    class IResultReader {
    public:
        virtual ~IResultReader() = default;

        virtual bool next() = 0;
        virtual bool isNull(std::string_view column) const = 0;
        virtual int32_t getInt(std::string_view column) const = 0;
        virtual uint32_t getUInt(std::string_view column) const = 0;
        virtual int64_t getInt64(std::string_view column) const = 0;
        virtual uint64_t getUInt64(std::string_view column) const = 0;
        virtual long double getDouble(std::string_view column) const = 0;
        virtual std::string getString(std::string_view column) const = 0;
        virtual std::unique_ptr<std::istream> getBlob(std::string_view column) const = 0;
    };
}
