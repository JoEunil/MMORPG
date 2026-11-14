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
        std::map<StatementId, std::unique_ptr<sql::PreparedStatement>> m_stmts;
        std::unique_ptr<sql::Connection> m_conn;
        std::unique_ptr<std::istringstream> m_blobStream; // blob 처리용도, dangling pointer 방지

        void Connect(const sql::SQLString& host, const sql::SQLString& user,
            const sql::SQLString& password, const sql::SQLString& database) {
            sql::ConnectOptionsMap options;
            options["hostName"] = host;
            options["userName"] = user;
            options["password"] = password;
            // UPDATE 반환값을 실제 변경 행 수에서 WHERE에 일치한 행 수로 바꾼다.
            // 이미 같은 값인 행도 양수를 반환하므로, 대상 행 없음(0)과 구분된다.
            options["CLIENT_FOUND_ROWS"] = true;
            sql::Driver* driver = sql::mysql::get_driver_instance();
            m_conn = std::unique_ptr<sql::Connection>(driver->connect(options));
            m_conn->setSchema(database);
        }
        
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
        void ClearResults() {
            for (auto& [id, stmt] : m_stmts) {
                while (stmt->getMoreResults()) {
                    std::unique_ptr<sql::ResultSet> extra(stmt->getResultSet());
                }
            }
        }

        virtual ~DBConnection() {

        }

    };
}
