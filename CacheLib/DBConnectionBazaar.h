#pragma once

#include "DBConnection.h"
#include "Queries.h"
namespace Cache
{
    template<typename T>
    class DBConnectionPool;
    class DBConnectionBazaar : public DBConnection {
        void Initialize() override {
            Connect(DB_HOST_BAZAAR, DB_USER_BAZAAR, DB_PASS_BAZAAR, DB_DB_BAZAAR);
            m_stmts[StatementId::SelectDiamond] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_1));
            m_stmts[StatementId::DepositDiamond] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_2));
            m_stmts[StatementId::SelectBazaarMyList] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_3));
            m_stmts[StatementId::SearchBazaar] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_4));
            m_stmts[StatementId::InsertBazaarListing] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_5));
            m_stmts[StatementId::CancelBazaarListing] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_6));
            m_stmts[StatementId::SelectBazaarListingItem] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_7));
            m_stmts[StatementId::BuyBazaarListing] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_8));
            m_stmts[StatementId::ClaimBazaarProceeds] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_9));
            m_stmts[StatementId::SelectBuyerOutbox] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_10));
            m_stmts[StatementId::ClaimBuyerOutboxEvent] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(BAZAAR_QUERY_11));
        }
		friend class DBConnectionPool<DBConnectionBazaar>;
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

            if (GetStatementType(stmt_id) != StatementType::BazaarQuery)
                return nullptr;

            try {
                return std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
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
        int ExecuteUpdate(StatementId stmt_id, Args&&... args) {
            auto it = m_stmts.find(stmt_id);
            if (it == m_stmts.end()) {
                return -1;
            }

            auto& stmt = it->second;
            BindParams(*stmt, 1, std::forward<Args>(args)...);

            if (GetStatementType(stmt_id) != StatementType::BazaarUpdate)
                return 0;

            return stmt->executeUpdate();
        }
    };
}
