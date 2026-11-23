#include "MySQLResultAdapter.h"

#include <stdexcept>
#include <utility>
#include <mysqlconn/include/jdbc/cppconn/resultset.h>

namespace Cache {
    MySQLResultAdapter::MySQLResultAdapter(std::unique_ptr<sql::ResultSet> result)
        : result_(std::move(result)) {
        if (!result_)
            throw std::invalid_argument("MySQL result must not be null.");
    }

    MySQLResultAdapter::~MySQLResultAdapter() = default;

    bool MySQLResultAdapter::next() {
        return result_->next();
    }

    bool MySQLResultAdapter::isNull(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return result_->isNull(label);
    }

    int32_t MySQLResultAdapter::getInt(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return result_->getInt(label);
    }

    uint32_t MySQLResultAdapter::getUInt(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return result_->getUInt(label);
    }

    int64_t MySQLResultAdapter::getInt64(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return result_->getInt64(label);
    }

    uint64_t MySQLResultAdapter::getUInt64(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return result_->getUInt64(label);
    }

    long double MySQLResultAdapter::getDouble(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return result_->getDouble(label);
    }

    std::string MySQLResultAdapter::getString(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return result_->getString(label).asStdString();
    }

    std::unique_ptr<std::istream> MySQLResultAdapter::getBlob(std::string_view column) const {
        const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
        return std::unique_ptr<std::istream>(result_->getBlob(label));
    }
}
