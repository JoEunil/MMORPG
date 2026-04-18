#pragma once

#include "DBConnection.h"
namespace Cache
{
    template<typename T>
    class DBConnectionPool;
    class DBConnectionBilling : public DBConnection {
        void Connect() {
            sql::SQLString host = DB_HOST_BILLING;
            sql::SQLString user = DB_USER_BILLING;
            sql::SQLString password = DB_PASS_BILLING;
            sql::SQLString database = DB_DB_BILLING;
            sql::Driver* driver = sql::mysql::get_driver_instance();
            m_conn = std::unique_ptr<sql::Connection>(driver->connect(host, user, password));
            m_conn->setSchema(database);
        }
        void Initialize() override {
            Connect();
            m_stmts[9] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_9));
            m_stmts[10] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_10));
            m_stmts[11] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_11));
            m_stmts[12] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_12));
            m_stmts[13] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_13));
            m_stmts[14] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_14));
            m_stmts[15] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_15));
            m_stmts[16] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_16));
            m_stmts[17] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_17));
        }
		friend class DBConnectionPool<DBConnectionBilling>;
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
            case 9: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 11: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 12: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 15: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 16: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 17: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
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
            case 10: return stmt->executeUpdate(); // UPDATE
            case 13: return stmt->executeUpdate(); // INSERT
            case 14: return stmt->executeUpdate(); // UPDATE
            default: return 0;
            }
        }
    };
}