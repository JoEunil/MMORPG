#pragma once

#include <mysqlconn/include/mysql/jdbc.h>
#include <cstdint>
#include <string_view>
#include <map>
#include <memory>

#include "Config.h"
#include <iostream>


namespace Cache {
    class DBConnection {
        std::unique_ptr<sql::Connection> m_conn;
        std::map<uint16_t, std::unique_ptr<sql::PreparedStatement>> m_stmts;
        void Initialize() {
            Connect();
            m_stmts[1] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_1));
            m_stmts[2] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_2));
            m_stmts[3] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_3));
            m_stmts[4] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_4));
            m_stmts[5] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_5));
        }
        void Connect() {
            sql::SQLString host = DB_HOST;
            sql::SQLString user = DB_USER;
            sql::SQLString password = DB_PASS;
            sql::SQLString database = DB_DB;
            sql::Driver* driver = sql::mysql::get_driver_instance();
            m_conn = std::unique_ptr<sql::Connection>(driver->connect(host, user, password));
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
        void BindOne(sql::PreparedStatement& stmt, int idx, std::vector<uint8_t>& value) { stmt.setString(idx, reinterpret_cast<const char*>(value.data())); }
        

        friend class DBConnectionPool;

    public:
        template<typename... Args>
        std::unique_ptr<sql::ResultSet> ExecuteSelect(uint16_t stmt_id, Args&&... args) {
            // 템플릿 인자를 T&&로 쓰면 인자 추론이 되어, r-value, l-value 둘 다 사용 가능
            auto it = m_stmts.find(stmt_id);
            if (it == m_stmts.end()) {
                return nullptr;
            }
            
            auto& stmt = it->second;
            BindParams(*stmt, 1, std::forward<Args>(args)...);
            
            switch(stmt_id)
            {
                case 1: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); // select
                case 3: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); // select
                case 5: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
                default: return nullptr;
            }
        }

        template<typename... Args>
        int ExecuteUpdate(uint16_t stmt_id, Args&&... args) {
            auto it = m_stmts.find(stmt_id);
            if (it == m_stmts.end()) {
                return -1;
            }
            
            auto& stmt = it->second;
            BindParams(*stmt, 1, std::forward<Args>(args)...);
            
            switch(stmt_id)
            {
                case 2: return stmt->executeUpdate(); // insert
                case 4: return stmt->executeUpdate(); // update
                case 6: return stmt->executeUpdate(); // update
                default: return 0;
            }
        }
    };
}
