#pragma once

#include <cstdint>
#include <string_view>
#include <map>
#include <memory>
#include <sstream>

#include <mysqlconn/include/mysql/jdbc.h>
#include <CoreLib/LoggerGlobal.h>
#include "Config.h"


namespace Cache {
    class DBConnection {
    protected:
        virtual void Initialize() = 0;
        std::map<uint16_t, std::unique_ptr<sql::PreparedStatement>> m_stmts;
        std::unique_ptr<sql::Connection> m_conn;
        std::unique_ptr<std::istringstream> m_blobStream; // blob 처리용도, dangling pointer 방지
        
        template<typename T, typename... Rest>
        void BindParams(sql::PreparedStatement& stmt, int idx, T&& value, Rest&&... rest) {
            BindOne(stmt, idx, std::forward<T>(value));
            if constexpr (sizeof...(rest) > 0)
                BindParams(stmt, idx + 1, std::forward<Rest>(rest)...);
        }
        // 재귀적으로 BindOne 호출

        void BindOne(sql::PreparedStatement& stmt, int idx, int value) { stmt.setInt(idx, value); }
        void BindOne(sql::PreparedStatement& stmt, int idx, int8_t value) { stmt.setInt(idx, static_cast<int>(value)); }

        void BindOne(sql::PreparedStatement& stmt, int idx, uint8_t value) { stmt.setUInt(idx, static_cast<unsigned int>(value)); }

        void BindOne(sql::PreparedStatement& stmt, int idx, int16_t value) { stmt.setInt(idx, static_cast<int>(value)); }
        void BindOne(sql::PreparedStatement& stmt, int idx, uint16_t value) { stmt.setUInt(idx, static_cast<unsigned int>(value)); }
        void BindOne(sql::PreparedStatement& stmt, int idx, uint32_t value) { stmt.setUInt64(idx, value); }
        void BindOne(sql::PreparedStatement& stmt, int idx, uint64_t value) { stmt.setUInt64(idx, value); }
        void BindOne(sql::PreparedStatement& stmt, int idx, float value) { stmt.setDouble(idx, value); }
        void BindOne(sql::PreparedStatement& stmt, int idx, const std::string& value) { stmt.setString(idx, value); }
        void BindOne(sql::PreparedStatement& stmt, int idx, std::vector<uint8_t>& value) {
            std::string blobData(reinterpret_cast<const char*>(value.data()), value.size());
            m_blobStream = std::make_unique<std::istringstream>(blobData);
            m_blobStream->seekg(0);
            stmt.setBlob(idx, m_blobStream.get()); // executeUpdate까지 살아있음
        }

    public:
        virtual ~DBConnection() {

        }

    };
}
