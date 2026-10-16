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
            m_stmts[1] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_1));
            m_stmts[2] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_2));
            m_stmts[3] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_3));
            m_stmts[4] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_4));
            m_stmts[5] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_5));
            m_stmts[6] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_6));
            m_stmts[7] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_7));
            m_stmts[8] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_8));
            m_stmts[20] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_20));
            m_stmts[21] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_21));
            m_stmts[22] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_22));
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
            case 21: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            default: return nullptr;
            }
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
            case 2: return stmt->executeUpdate(); // insert
            case 4: return stmt->executeUpdate(); // update
            case 6: return stmt->executeUpdate(); // update
            case 8: return stmt->executeUpdate(); // update
            case 20: return stmt->executeUpdate(); // insert
            case 22: return stmt->executeUpdate(); // update
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