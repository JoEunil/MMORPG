#pragma once

#include "DBConnection.h"
namespace Cache
{
    template<typename T>
    class DBConnectionPool;
    class DBConnectionBazaar : public DBConnection {
        void Connect() {
            sql::SQLString host = DB_HOST_BAZAAR;
            sql::SQLString user = DB_USER_BAZAAR;
            sql::SQLString password = DB_PASS_BAZAAR;
            sql::SQLString database = DB_DB_BAZAAR;
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
            m_stmts[18] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_18));
            m_stmts[19] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_19));
        }
		friend class DBConnectionPool<DBConnectionBazaar>;
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
            try {
            switch (stmt_id)
            {
            case 9: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 11: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 12: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 15: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 16: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 17: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case 18: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            default: return nullptr;
            }
              }
            catch (const sql::SQLException& e) {
                std::cerr << "SQL Exception: " << e.what() << "\n";
                std::cerr << "Error code: " << e.getErrorCode() << "\n";
                std::cerr << "SQL state: " << e.getSQLState() << "\n";
            }
            catch (const std::exception& e) {
                std::cerr << "Exception: " << e.what() << "\n";
            }
            return nullptr;
        }

        // 지원하는 UPDATE: >0은 대상 행 있음(동일 값 포함), 0은 WHERE 일치 행 없음.
        // INSERT: 삽입된 행 수. -1은 stmt_id 미등록, 등록됐지만 미지원이면 0.
        // SQL 실행 오류는 sql::SQLException으로 호출자에게 전파한다.
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
            case 19: return stmt->executeUpdate(); // UPDATE
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