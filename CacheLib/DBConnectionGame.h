#pragma once

#include "DBConnection.h"
#include "Queries.h"
namespace Cache
{
    template<typename T>
    class DBConnectionPool;
    class DBConnectionGame : public DBConnection {
        void Initialize() override {
            Connect(DB_HOST_GAME, DB_USER_GAME, DB_PASS_GAME, DB_DB_GAME);
            for (const auto& [id, sql] : GAME_QUERIES) {
                m_stmts[id] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(sql));
            }
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

            if (GetStatementType(stmt_id) != StatementType::GameQuery)
                return nullptr;

            return std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
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

            if (GetStatementType(stmt_id) != StatementType::GameUpdate)
                return 0;

            return stmt->executeUpdate();
        }
    };
}
