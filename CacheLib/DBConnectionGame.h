#pragma once

#include "DBConnection.h"
namespace Cache
{
    template<typename T>
    class DBConnectionPool;
    class DBConnectionGame : public DBConnection {
        void Connect() {
            sql::SQLString host = DB_HOST_GAME;
            sql::SQLString user = DB_USER_GAME;
            sql::SQLString password = DB_PASS_GAME;
            sql::SQLString database = DB_DB_GAME;
            sql::Driver* driver = sql::mysql::get_driver_instance();
            m_conn = std::unique_ptr<sql::Connection>(driver->connect(host, user, password));
            m_conn->setSchema(database);
        }
        void Initialize() override {
            Connect();
            m_stmts[1] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_1));
            m_stmts[2] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_2));
            m_stmts[3] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_3));
            m_stmts[4] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_4));
            m_stmts[5] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_5));
            m_stmts[6] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_6));
            m_stmts[7] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_7));
            m_stmts[8] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_8));
        }
        friend class DBConnectionPool<DBConnectionGame>;
    public:
        template<typename... Args>
        std::unique_ptr<sql::ResultSet> ExecuteSelect(uint16_t stmt_id, Args&&... args) {
            // 템플릿 인자를 && 로 쓰면 forwarding reference
            auto it = m_stmts.find(stmt_id);
            if (it == m_stmts.end()) {
                return nullptr;
            }

            auto& stmt = it->second;
            BindParams(*stmt, 1, std::forward<Args>(args)...);

            switch (stmt_id)
            {
            case 1: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); // select
            case 3: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); // select
            case 5: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 7: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
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

            switch (stmt_id)
            {
            case 2: return stmt->executeUpdate(); // insert
            case 4: return stmt->executeUpdate(); // update
            case 6: return stmt->executeUpdate(); // update
            case 8: return stmt->executeUpdate(); // update
            default: return 0;
            }
        }

        void ClearResults() {
            for (auto& [id, stmt] : m_stmts) {
                while (stmt->getMoreResults()) {
                    std::unique_ptr<sql::ResultSet> extra(stmt->getResultSet());
                }
            }
        }
    };
}