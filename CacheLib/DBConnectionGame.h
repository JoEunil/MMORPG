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
            m_stmts[StatementId::SelectCharacterList] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_1));
            m_stmts[StatementId::InsertCharacter] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_2));
            m_stmts[StatementId::SelectCharacterState] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_3));
            m_stmts[StatementId::UpdateCharacterState] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_4));
            m_stmts[StatementId::SelectInventory] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_5));
            m_stmts[StatementId::UpdateInventory] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_6));
            m_stmts[StatementId::SelectCurrency] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_7));
            m_stmts[StatementId::UpsertCurrency] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_8));
            m_stmts[StatementId::InsertProfile] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_20));
            m_stmts[StatementId::SelectProfile] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_21));
            m_stmts[StatementId::RenameProfile] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_22));
        }
        friend class DBConnectionPool<DBConnectionGame>;
    public:
        template<typename... Args>
        std::unique_ptr<sql::ResultSet> ExecuteSelect(StatementId stmt_id, Args&&... args) {
            // 템플릿 인자를 && 로 쓰면 forwarding reference
            auto it = m_stmts.find(stmt_id);
            if (it == m_stmts.end()) {
                return nullptr;
            }

            auto& stmt = it->second;
            BindParams(*stmt, 1, std::forward<Args>(args)...);

            switch (stmt_id)
            {
            case StatementId::SelectCharacterList: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); // select
            case StatementId::SelectCharacterState: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); // select
            case StatementId::SelectInventory: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::SelectCurrency: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::SelectProfile: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            default: return nullptr;
            }
        }

        // 지원하는 UPDATE: >0은 대상 행 있음(동일 값 포함), 0은 WHERE 일치 행 없음.
        // INSERT: 삽입된 행 수. -1은 stmt_id 미등록, 등록됐지만 미지원이면 0.
        // SQL 실행 오류는 sql::SQLException으로 호출자에게 전파한다.
        template<typename... Args>
        int ExecuteUpdate(StatementId stmt_id, Args&&... args) {
            auto it = m_stmts.find(stmt_id);
            if (it == m_stmts.end()) {
                return -1;
            }

            auto& stmt = it->second;
            BindParams(*stmt, 1, std::forward<Args>(args)...);

            switch (stmt_id)
            {
            case StatementId::InsertCharacter: return stmt->executeUpdate(); // insert
            case StatementId::UpdateCharacterState: return stmt->executeUpdate(); // update
            case StatementId::UpdateInventory: return stmt->executeUpdate(); // update
            case StatementId::UpsertCurrency: return stmt->executeUpdate(); // update
            case StatementId::InsertProfile: return stmt->executeUpdate(); // insert
            case StatementId::RenameProfile: return stmt->executeUpdate(); // update
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