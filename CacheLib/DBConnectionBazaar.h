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
            m_stmts[StatementId::SelectDiamond] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_9));
            m_stmts[StatementId::DepositDiamond] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_10));
            m_stmts[StatementId::SelectBazaarMyList] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_11));
            m_stmts[StatementId::SearchBazaar] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_12));
            m_stmts[StatementId::InsertBazaarListing] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_13));
            m_stmts[StatementId::CancelBazaarListing] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_14));
            m_stmts[StatementId::SelectBazaarListingItem] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_15));
            m_stmts[StatementId::BuyBazaarListing] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_16));
            m_stmts[StatementId::ClaimBazaarProceeds] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_17));
            m_stmts[StatementId::SelectBuyerOutbox] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_18));
            m_stmts[StatementId::ClaimBuyerOutboxEvent] = std::unique_ptr<sql::PreparedStatement>(m_conn->prepareStatement(QUERY_19));
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
            try {
            switch (stmt_id)
            {
            case StatementId::SelectDiamond: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::SelectBazaarMyList: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::SearchBazaar: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::CancelBazaarListing: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); // cancel transaction
            case StatementId::SelectBazaarListingItem: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::BuyBazaarListing: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::ClaimBazaarProceeds: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
            case StatementId::SelectBuyerOutbox: return std::unique_ptr<sql::ResultSet>(stmt->executeQuery()); //select
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
        int ExecuteUpdate(StatementId stmt_id, Args&&... args) {
            auto it = m_stmts.find(stmt_id);
            if (it == m_stmts.end()) {
                return -1;
            }

            auto& stmt = it->second;
            BindParams(*stmt, 1, std::forward<Args>(args)...);

            switch (stmt_id)
            {
            case StatementId::DepositDiamond: return stmt->executeUpdate(); // UPDATE
            case StatementId::InsertBazaarListing: return stmt->executeUpdate(); // INSERT
            case StatementId::ClaimBuyerOutboxEvent: return stmt->executeUpdate(); // UPDATE
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