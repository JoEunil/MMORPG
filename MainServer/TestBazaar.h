#pragma once
#include <memory>
#include <iostream>
#include <format>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <vector>
#include <cstdint>
#include <mysqlconn/include/mysql/jdbc.h>

#include <CacheLib/Initializer.h>
#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/MessagePool.h>
#include <CoreLib/ItemData.h>
#include <CacheLib/Config.h>

// ================================================================
// 크래시 포인트 매크로 (BazaarHandler.cpp 상단에 위치)
// 테스트 시 주석 해제 후 빌드, 완료 후 다시 주석 처리
// #define CRASH_POINT_BUY     // 프로시저 성공 후 PartialUpdate 전
// #define CRASH_POINT_CANCEL  // DB CANCEL 처리 후 PartialUpdate 전
// ================================================================

namespace Test {
    inline Core::MessagePool& g_msgPool() {
        static Core::MessagePool pool;
        return pool;
    }
    struct CacheNode;
    static void WarmUp(CacheNode& node, uint64_t charID, uint64_t sessionID);
    static void SafeReturn(Core::Message* msg);
    // ----------------------------------------------------------------
    // MockMessageQueue
    // ----------------------------------------------------------------
    class MockMessageQueue : public Core::IMessageQueue {
        std::mutex                 m_mutex;
        std::condition_variable    m_cv;
        std::queue<Core::Message*> m_received;
    public:
        void EnqueueMessage(Core::Message* msg) override {
            auto* copy = g_msgPool().Acquire();
            std::memcpy(copy->GetBuffer(), msg->GetBuffer(), msg->GetLength());
            copy->SetLength(msg->GetLength());
            std::lock_guard lock(m_mutex);
            m_received.push(copy);
            m_cv.notify_one();
        }
        Core::Message* WaitFor(int ms = 5000) {
            std::unique_lock lock(m_mutex);
            if (m_cv.wait_for(lock, std::chrono::milliseconds(ms),
                [this] { return !m_received.empty(); })) {
                auto* msg = m_received.front();
                m_received.pop();
                return msg;
            }
            return nullptr;
        }
    };

    // ----------------------------------------------------------------
    // 상수
    // DB에 캐릭터 11~5010 미리 생성, currency/diamond 할당됨
    // ----------------------------------------------------------------
    inline constexpr int      MSG_LEN = 512;
    inline constexpr uint64_t SESSION_A = 1004; // seller
    inline constexpr uint64_t SESSION_B = 1005; // buyer1
    inline constexpr uint64_t SESSION_C = 1006; // buyer2
    inline constexpr uint64_t CHAR_A = 50;   // seller
    inline constexpr uint64_t CHAR_B = 51;   // buyer1
    inline constexpr uint64_t CHAR_C = 52;   // buyer2
    inline constexpr uint64_t CHAR_LOAD_BASE = 60;   // 부하테스트 buyer 시작 ID (20~29)

    // ----------------------------------------------------------------
    // 헬퍼
    // ----------------------------------------------------------------
    template<typename BodyType>
    static Core::Message* MakeMsg(uint16_t msgType, uint64_t sessionID, const BodyType& body) {
		auto* msg = g_msgPool().Acquire();
        auto* st = reinterpret_cast<Core::MsgStruct<BodyType>*>(msg->GetBuffer());
        st->header.messageType = msgType;
        st->header.sessionID = sessionID;
        st->body = body;
        msg->SetLength(sizeof(Core::MsgStruct<BodyType>));
        return msg;
    }

    template<typename ResBody>
    static const ResBody* GetBody(Core::Message* msg) {
        return &reinterpret_cast<Core::MsgStruct<ResBody>*>(msg->GetBuffer())->body;
    }

    // Cache::Initializer + MockMessageQueue 묶음
    struct CacheNode {
        MockMessageQueue   mq;
        Cache::Initializer cache;

        CacheNode() {
            cache.Initialize();
            cache.InjectDependencies(&mq);
        }
        ~CacheNode() {
        }
        void Send(Core::Message* msg) {
            cache.GetMessageQueue()->EnqueueMessage(msg);
        }
        Core::Message* Wait(int ms = 5000) {
            return mq.WaitFor(ms);
        }
    };

    // ----------------------------------------------------------------
    // DB 상태 체크
    // ----------------------------------------------------------------
    struct DBChecker {
        sql::Connection* conn = nullptr;

        DBChecker() {
            auto* driver = sql::mysql::get_mysql_driver_instance();
            conn = driver->connect(
                Cache::DB_HOST_BILLING,
                Cache::DB_USER_BILLING,
                Cache::DB_PASS_BILLING);
            conn->setSchema(Cache::DB_DB_BILLING);
        }
        ~DBChecker() { delete conn; }

        void PrintListing(uint64_t listingID) {
            std::unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
                "SELECT listing_id, item_id, quantity, status, buyer_id FROM bazaar WHERE listing_id = ?"));
            ps->setUInt64(1, listingID);
            std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
            if (rs->next())
                std::cout << std::format("  [bazaar] listing={} item={} qty={} status={} buyer={}\n",
                    rs->getUInt64("listing_id"), rs->getUInt64("item_id"),
                    rs->getUInt("quantity"), rs->getString("status").c_str(),
                    rs->getUInt64("buyer_id"));
            else
                std::cout << std::format("  [bazaar] listing={} 없음\n", listingID);
        }

        void PrintDiamond(uint64_t charID) {
            std::unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
                "SELECT char_id, diamond FROM characters_diamond WHERE char_id = ?"));
            ps->setUInt64(1, charID);
            std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
            if (rs->next())
                std::cout << std::format("  [diamond] char={}  diamond={}\n",
                    charID,  rs->getUInt64("diamond"));
            else
                std::cout << std::format("  [diamond] char={} 없음\n", charID);
        }

        void PrintBazaarLog(uint64_t listingID) {
            std::unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
                "SELECT log_id, seller_id, buyer_id, item_type, price FROM bazaar_log WHERE listing_id = ?"));
            ps->setUInt64(1, listingID);
            std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
            if (rs->next())
                std::cout << std::format("  [bazaar_log] log={} seller={} buyer={} item_type={} price={}\n",
                    rs->getUInt64("log_id"), rs->getUInt64("seller_id"),
                    rs->getUInt64("buyer_id"), rs->getUInt("item_type"), rs->getUInt64("price"));
            else
                std::cout << std::format("  [bazaar_log] listing={} 기록 없음\n", listingID);
        }

        uint64_t FetchLatestListingID(uint64_t charID) {
            auto s = std::unique_ptr<sql::PreparedStatement>(conn->prepareStatement(
                "SELECT listing_id FROM bazaar "
                "WHERE seller_id = ? ORDER BY listing_id DESC LIMIT 1"));
            s->setUInt64(1, charID);
            auto res = std::unique_ptr<sql::ResultSet>(s->executeQuery());
            if (res && res->next()) return res->getUInt64("listing_id");
            return 0;
        }

        uint64_t QuerySingle(const char* like) {
            std::unique_ptr<sql::Statement> st(conn->createStatement());
            std::unique_ptr<sql::ResultSet> rs(st->executeQuery(
                std::format("SHOW GLOBAL STATUS LIKE '{}'", like)));
            if (rs->next()) return std::stoull(rs->getString("Value").c_str());
            return 0;
        }

        struct StatusSnapshot {
            uint64_t commit = 0;
            uint64_t rollback = 0;
            uint64_t lockWaits = 0;
        };
        StatusSnapshot Snapshot() {
            auto query = [&](const char* like) -> uint64_t {
                std::unique_ptr<sql::Statement> st(conn->createStatement());
                std::unique_ptr<sql::ResultSet> rs(st->executeQuery(
                    std::format("SHOW GLOBAL STATUS LIKE '{}'", like)));
                if (rs->next()) return std::stoull(rs->getString("Value").c_str());
                return 0;
                };
            return { query("Com_commit"), query("Com_rollback"), query("Innodb_row_lock_waits") };
        }
        void PrintDiff(const StatusSnapshot& before, const StatusSnapshot& after) {
            std::cout << std::format("  [DB stat] commit+={} rollback+={} lock_waits+={}\n",
                after.commit - before.commit,
                after.rollback - before.rollback,
                after.lockWaits - before.lockWaits);
        }

        void CleanupBazaar() {
            std::unique_ptr<sql::Statement> st(conn->createStatement());
            st->execute("DELETE FROM bazaar_log WHERE seller_id BETWEEN 11 AND 30 OR buyer_id BETWEEN 11 AND 30");
            st->execute("DELETE FROM bazaar WHERE seller_id BETWEEN 11 AND 30");
        }
    };

    // ================================================================
    // 테스트 케이스
    // ================================================================

    // ----------------------------------------------------------------
    // 1. 인벤토리 아이템 추가/제거
    // ----------------------------------------------------------------
    inline void Test1_Inventory() {
        std::cout << "\n=== [1] 인벤토리 추가/제거 ===\n";
        CacheNode node;
        WarmUp(node, CHAR_A, SESSION_A);


        // 추가 (HP포션 5개)
        Core::MsgInventoryUpdateBody addBody{};
        addBody.characterID = CHAR_A;
        addBody.itemID = 1;
        addBody.op = 1;
        addBody.change = 5;
        auto msg = MakeMsg(Core::MSG_INVENTORY_UPDATE, SESSION_A, addBody);
        node.Send(msg);
		SafeReturn(msg);
        auto* res = node.Wait();
        auto* addRes = GetBody<Core::MsgInventoryUpdateResBody>(res);
        std::cout << std::format("  추가: status={} itemID={} qty={}\n",
            addRes->resStatus, addRes->itemID, addRes->itemQuantity);
        SafeReturn(res);

        // 제거 (HP포션 2개)
        Core::MsgInventoryUpdateBody removeBody{};
        removeBody.characterID = CHAR_A;
        removeBody.itemID = 1;
        removeBody.op = 2;
        removeBody.change = -2;
        auto msg2 = MakeMsg(Core::MSG_INVENTORY_UPDATE, SESSION_A, removeBody);
        node.Send(msg2);
        SafeReturn(msg2);
        res = node.Wait();
        auto* removeRes = GetBody<Core::MsgInventoryUpdateResBody>(res);
        std::cout << std::format("  제거: status={} itemID={} qty={}\n",
            removeRes->resStatus, removeRes->itemID, removeRes->itemQuantity);
        SafeReturn(res);

        // 조회 (3개 남아있어야 함)
        Core::MsgInventoryReqBody reqBody{};
        reqBody.characterID = CHAR_A;
        auto msg3 = MakeMsg(Core::MSG_INVENTORY_REQ, SESSION_A, reqBody);
        node.Send(msg3);
        SafeReturn(msg3);
        res = node.Wait();
        auto* invRes = GetBody<Core::MsgInventoryResBody>(res);
        std::cout << std::format("  조회: status={} count={}\n", invRes->resStatus, invRes->itemCount);
        for (int i = 0; i < invRes->itemCount; ++i)
            std::cout << std::format("    slot={} itemID={} qty={}\n",
                invRes->items[i].slot, invRes->items[i].itemID, invRes->items[i].quantity);
        SafeReturn(res);
    }

    // ----------------------------------------------------------------
    // 2. Currency (Gold) 추가/조회
    // ----------------------------------------------------------------
    inline void Test2_Currency() {
        std::cout << "\n=== [2] Currency (Gold) 추가/조회 ===\n";
        CacheNode node;
        WarmUp(node, CHAR_A, SESSION_A);

        Core::MsgCurrencyReqBody reqBody{};
        reqBody.characterID = CHAR_A;
        auto msg1 = MakeMsg(Core::MSG_CURRENCY_REQ, SESSION_A, reqBody);
        node.Send(msg1);
        SafeReturn(msg1);
        auto* res1 = node.Wait();
        auto* reqRes = GetBody<Core::MsgCurrencyResBody>(res1);
        std::cout << std::format("  조회: status={} gold={}\n", reqRes->resStatus, reqRes->gold);
        SafeReturn(res1);

        Core::MsgCurrencyDepositBody depositBody{};
        depositBody.characterID = CHAR_A;
        depositBody.gold = 10000;
        auto msg2 = MakeMsg(Core::MSG_CURRENCY_DEPOSIT, SESSION_A, depositBody);
        node.Send(msg2);
        SafeReturn(msg2);
        auto* res2 = node.Wait();
        auto* depositRes = GetBody<Core::MsgCurrencyDepositResBody>(res2);
        std::cout << std::format("  입금: status={} gold={}\n", depositRes->resStatus, depositRes->gold);
        SafeReturn(res2);

    }

    // ----------------------------------------------------------------
    // 3. Diamond 추가/조회
    // ----------------------------------------------------------------
    inline void Test3_Diamond() {
        std::cout << "\n=== [3] Diamond 추가/조회 ===\n";
        CacheNode node;
        WarmUp(node, CHAR_A, SESSION_A);

        Core::MsgDiamondReqBody reqBody{};
        reqBody.characterID = CHAR_A;
        auto msg2 = MakeMsg(Core::MSG_DIAMOND_REQ, SESSION_A, reqBody);
        node.Send(msg2);
        SafeReturn(msg2);
        auto* res1 = node.Wait();
        auto* reqRes = GetBody<Core::MsgDiamondResBody>(res1);
        std::cout << std::format("  조회: status={} diamond={} earned={} spent={}\n",reqRes->resStatus, reqRes->diamond, reqRes->totalEarned, reqRes->totalSpent);
        SafeReturn(res1);

        Core::MsgDiamondDepositBody depositBody{};
        depositBody.characterID = CHAR_A;
        depositBody.diamond = 500;
        auto msg = MakeMsg(Core::MSG_DIAMOND_DEPOSIT, SESSION_A, depositBody);
        node.Send(msg);
        SafeReturn(msg);
        auto* res2 = node.Wait();
        auto* depositRes = GetBody<Core::MsgDiamondDepositResBody>(res2);
        std::cout << std::format("  입금: status={} diamond={}\n", depositRes->resStatus, depositRes->diamond);
        SafeReturn(res2);

    }

    // ----------------------------------------------------------------
    // 4. Bazaar 기본 동작 (REGISTER → BUY → CLAIM)
    // ----------------------------------------------------------------
    inline void Test4_BazaarBasic() {
        std::cout << "\n=== [4] Bazaar 기본 동작 (REGISTER → BUY → CLAIM) ===\n";

        DBChecker db;
        db.CleanupBazaar();

        CacheNode seller;
        CacheNode buyer;
        WarmUp(seller, CHAR_A, SESSION_A);
        WarmUp(buyer, CHAR_B, SESSION_B);

        // seller: 철검 1개 추가
        Core::MsgInventoryUpdateBody addItem{};
        addItem.characterID = CHAR_A;
        addItem.itemID = 3; // 철검
        addItem.op = 1;
        addItem.change = 1;
        auto msg = MakeMsg(Core::MSG_INVENTORY_UPDATE, SESSION_A, addItem);
        seller.Send(msg);
        SafeReturn(msg);
        SafeReturn(seller.Wait());

        // buyer: 다이아 충전
        Core::MsgDiamondDepositBody diamondDeposit{};
        diamondDeposit.characterID = CHAR_B;
        diamondDeposit.diamond = 1000;
        auto msg2 = MakeMsg(Core::MSG_DIAMOND_DEPOSIT, SESSION_B, diamondDeposit);
        buyer.Send(msg2);
        SafeReturn(msg2);
        SafeReturn(buyer.Wait());

        std::cout << "  [등록 전]\n";
        db.PrintDiamond(CHAR_A);
        db.PrintDiamond(CHAR_B);

        // REGISTER
        Core::MsgBazaarRegisterBody regBody{};
        regBody.characterID = CHAR_A;
        regBody.itemID = 3;
        regBody.quantity = 1;
        regBody.price = 100;
        auto msg3 = MakeMsg(Core::MSG_BAZAAR_REGISTER, SESSION_A, regBody);
        seller.Send(msg3);
        SafeReturn(msg3);
        auto* regRes_raw = seller.Wait();
        uint64_t listingID = db.FetchLatestListingID(CHAR_A);
        std::cout << std::format("  REGISTER: status={} listingID={}\n",
            GetBody<Core::MsgBazaarRegisterResBody>(regRes_raw)->resStatus, listingID);
        SafeReturn(regRes_raw);
        db.PrintListing(listingID);

        // BUY
        Core::MsgBazaarBuyBody buyBody{};
        buyBody.characterID = CHAR_B;
        buyBody.listingID = listingID;
        auto msg4 = MakeMsg(Core::MSG_BAZAAR_BUY, SESSION_B, buyBody);
        buyer.Send(msg4);
        SafeReturn(msg4);
        auto* buyRes_raw = buyer.Wait();
        auto* buyRes = GetBody<Core::MsgBazaarBuyResBody>(buyRes_raw);
        std::cout << std::format("  BUY: status={} itemID={} qty={} spent={}\n",
            buyRes->resStatus, buyRes->itemID, buyRes->quantity, buyRes->diamondSpent);
        SafeReturn(buyRes_raw);
        db.PrintListing(listingID);
        db.PrintBazaarLog(listingID);

        // CLAIM
        Core::MsgBazaarClaimBody claimBody{};
        claimBody.characterID = CHAR_A;
        claimBody.listingID = listingID;
        auto msg5 = MakeMsg(Core::MSG_BAZAAR_CLAIM, SESSION_A, claimBody);
        seller.Send(msg5);
        SafeReturn(msg5);
        auto* claimRes_raw = seller.Wait();
        auto* claimRes = GetBody<Core::MsgBazaarClaimResBody>(claimRes_raw);
        std::cout << std::format("  CLAIM: status={} diamond={}\n",
            claimRes->resStatus, claimRes->diamondClaimed);
        SafeReturn(claimRes_raw);

        std::cout << "  [완료 후]\n";
        db.PrintDiamond(CHAR_A); // 다이아 증가 확인
        db.PrintDiamond(CHAR_B); // 다이아 감소 확인
    }

    // ----------------------------------------------------------------
    // 5. Seller 종료 후 Buyer BUY + Seller 재접속 CLAIM
    // ----------------------------------------------------------------
    inline void Test5_SellerOffline() {
        std::cout << "\n=== [5] Seller 종료 후 동작 ===\n";

        DBChecker db;
        db.CleanupBazaar();

        uint64_t listingID = 0;

        // seller: REGISTER 후 종료
        {
            CacheNode seller;
            WarmUp(seller, CHAR_A, SESSION_A);
            Core::MsgInventoryUpdateBody addItem{};
            addItem.characterID = CHAR_A;
            addItem.itemID = 4; // 철방패
            addItem.op = 1;
            addItem.change = 1;
            auto msg1 = MakeMsg(Core::MSG_INVENTORY_UPDATE, SESSION_A, addItem);
            seller.Send(msg1);
            SafeReturn(msg1);

            SafeReturn(seller.Wait());

            Core::MsgBazaarRegisterBody regBody{};
            regBody.characterID = CHAR_A;
            regBody.itemID = 4;
            regBody.quantity = 1;
            regBody.price = 200;
            auto msg2 = MakeMsg(Core::MSG_BAZAAR_REGISTER, SESSION_A, regBody);
            seller.Send(msg2);
            SafeReturn(msg2);

            auto* regRes_raw = seller.Wait();
            listingID = db.FetchLatestListingID(CHAR_A);
            std::cout << std::format("  REGISTER: listingID={}\n", listingID);
            SafeReturn(regRes_raw);
            // CacheNode 소멸 → CleanUp
        }
        std::cout << "  Seller 종료\n";
        db.PrintListing(listingID);

        // buyer: seller 없는 상태에서 BUY
        {
            CacheNode buyer;
            WarmUp(buyer, CHAR_B, SESSION_B);
            Core::MsgDiamondDepositBody diamondDeposit{};
            diamondDeposit.characterID = CHAR_B;
            diamondDeposit.diamond = 1000;
            auto msg1 = MakeMsg(Core::MSG_DIAMOND_DEPOSIT, SESSION_B, diamondDeposit);
            buyer.Send(msg1);
            SafeReturn(msg1);
            SafeReturn(buyer.Wait());

            Core::MsgBazaarBuyBody buyBody{};
            buyBody.characterID = CHAR_B;
            buyBody.listingID = listingID;
            auto msg2 = MakeMsg(Core::MSG_BAZAAR_BUY, SESSION_B, buyBody);
            buyer.Send(msg2);
            SafeReturn(msg2);
            auto* buyRes_raw = buyer.Wait();
            std::cout << std::format("  BUY (seller 오프라인): status={}\n",
                GetBody<Core::MsgBazaarBuyResBody>(buyRes_raw)->resStatus);
            SafeReturn(buyRes_raw);
        }
        db.PrintListing(listingID);

        // seller: 재접속 후 CLAIM
        {
            CacheNode seller;
            WarmUp(seller, CHAR_A, SESSION_A);
            Core::MsgBazaarClaimBody claimBody{};
            claimBody.characterID = CHAR_A;
            claimBody.listingID = listingID;
            auto msg = MakeMsg(Core::MSG_BAZAAR_CLAIM, SESSION_A, claimBody);
            seller.Send(msg);
            SafeReturn(msg);
            auto* claimRes_raw = seller.Wait();
            auto* claimRes = GetBody<Core::MsgBazaarClaimResBody>(claimRes_raw);
            std::cout << std::format("  CLAIM (재접속): status={} diamond={}\n",
                claimRes->resStatus, claimRes->diamondClaimed);
            SafeReturn(claimRes_raw);
        }

        std::cout << "  [완료 후]\n";
        db.PrintDiamond(CHAR_A);
        db.PrintDiamond(CHAR_B);
    }

    // ----------------------------------------------------------------
    // 6. Crash 테스트
    // BazaarHandler.cpp 상단에서 매크로 주석 해제 후 빌드
    //
    // [BUY 크래시]
    //   기대: DB status=SOLD, bazaar_log 기록 있음, buyer 인벤토리 아이템 없음
    //   복구: bazaar_log 기반 수동 복구 가능
    //
    // ----------------------------------------------------------------
    inline void Test6_Crash() {
        std::cout << "\n=== [6] Crash 테스트 ===\n";
        std::cout << "  BazaarHandler.cpp 매크로 확인\n";

        DBChecker db;
        db.CleanupBazaar();

        uint64_t listingID = 0;

        // seller REGISTER
        {
            CacheNode seller;
            WarmUp(seller, CHAR_A, SESSION_A);
            Core::MsgInventoryUpdateBody addItem{};
            addItem.characterID = CHAR_A;
            addItem.itemID = 3;
            addItem.op = 1;
            addItem.change = 1;
            auto msg1 = MakeMsg(Core::MSG_INVENTORY_UPDATE, SESSION_A, addItem);
            seller.Send(msg1);
            SafeReturn(msg1);
            SafeReturn(seller.Wait());

            Core::MsgBazaarRegisterBody regBody{};
            regBody.characterID = CHAR_A;
            regBody.itemID = 3;
            regBody.quantity = 1;
            regBody.price = 100;
            auto msg2 = MakeMsg(Core::MSG_BAZAAR_REGISTER, SESSION_A, regBody);
            seller.Send(msg2);
            SafeReturn(msg2);
            auto* regRes_raw = seller.Wait();
            std::cout << "Registered\n";
            SafeReturn(regRes_raw);
        }

        listingID = db.FetchLatestListingID(CHAR_A);
        std::cout << "CHAR_B  다이아 차감 전\n";
        db.PrintDiamond(CHAR_B);
        db.PrintListing(listingID);
        db.PrintBazaarLog(listingID); // 기록 있어야 복구 가능

        {
            CacheNode node;
            Core::MsgInventoryReqBody reqBody{};
            WarmUp(node, CHAR_B, SESSION_B);
            reqBody.characterID = CHAR_B;
            auto msg3 = MakeMsg(Core::MSG_INVENTORY_REQ, SESSION_A, reqBody);
            node.Send(msg3);
            SafeReturn(msg3);
            auto res = node.Wait();
            auto* invRes = GetBody<Core::MsgInventoryResBody>(res);
            std::cout << std::format("CHAR_B inventory  조회: status = {} count = {}\n", invRes->resStatus, invRes->itemCount);
            for (int i = 0; i < invRes->itemCount; ++i)
                std::cout << std::format("    slot={} itemID={} qty={}\n",
                    invRes->items[i].slot, invRes->items[i].itemID, invRes->items[i].quantity);
        }

        // buyer BUY → CRASH_POINT_BUY 활성화 시 abort()
        {
            CacheNode buyer;

            Core::MsgBazaarBuyBody buyBody{};
            buyBody.characterID = CHAR_B;
            buyBody.listingID = listingID;
            auto msg2 = MakeMsg(Core::MSG_BAZAAR_BUY, SESSION_B, buyBody);
            buyer.Send(msg2);
            SafeReturn(msg2);

            // CRASH_POINT_BUY 활성화 시 abort() → 아래 도달 안 함
            auto* res = buyer.Wait(3000);
            if (!res)
                std::cout << "  abort() 발생 또는 타임아웃\n";
            else
                SafeReturn(res);
        }

        std::cout << "CHAR_B  다이아 차감 후\n";
        db.PrintDiamond(CHAR_B);     // 다이아 차감 확인 (트랜잭션이므로 차감됨)
        db.PrintListing(listingID);
        db.PrintBazaarLog(listingID); // 기록 있어야 복구 가능
        {
            CacheNode node;
            WarmUp(node, CHAR_B, SESSION_B);
            Core::MsgInventoryReqBody reqBody{};
            reqBody.characterID = CHAR_B;
            auto msg3 = MakeMsg(Core::MSG_INVENTORY_REQ, SESSION_A, reqBody);
            node.Send(msg3);
            SafeReturn(msg3);
            auto res = node.Wait();
            auto* invRes = GetBody<Core::MsgInventoryResBody>(res);
            std::cout << std::format("CHAR_B inventory  조회: status = {} count = {}\n", invRes->resStatus, invRes->itemCount);
            for (int i = 0; i < invRes->itemCount; ++i)
                std::cout << std::format("    slot={} itemID={} qty={}\n",
                    invRes->items[i].slot, invRes->items[i].itemID, invRes->items[i].quantity);
        }
    }

    // ----------------------------------------------------------------
    // 7. 동시 구매 경쟁
    // ----------------------------------------------------------------
    inline void Test7_Race() {
        std::cout << "\n=== [7] 동시 구매 경쟁 ===\n";

        constexpr int BUYER_COUNT = 10; // 동시 buyer (CHAR 20~29)
        constexpr int LISTING_COUNT = 5;  // 경쟁 listing 수

        DBChecker db;
        db.CleanupBazaar();

        // seller: listing LISTING_COUNT개 등록
        std::vector<uint64_t> listingIDs;
        {
            CacheNode seller;
            WarmUp(seller, CHAR_A, SESSION_A);
            for (int i = 0; i < LISTING_COUNT; ++i) {
                Core::MsgInventoryUpdateBody addItem{};
                addItem.characterID = CHAR_A;
                addItem.itemID = 1; // HP포션
                addItem.op = 1;
                addItem.change = 1;
                auto msg1 = MakeMsg(Core::MSG_INVENTORY_UPDATE, SESSION_A, addItem);
                seller.Send(msg1);
                SafeReturn(msg1);
                SafeReturn(seller.Wait());

                Core::MsgBazaarRegisterBody regBody{};
                regBody.characterID = CHAR_A;
                regBody.itemID = 1;
                regBody.quantity = 1;
                regBody.price = 50;
                auto msg2 = MakeMsg(Core::MSG_BAZAAR_REGISTER, SESSION_A, regBody);
                seller.Send(msg2);
                SafeReturn(msg2);
                auto* regRes_raw = seller.Wait();
                auto listingID = db.FetchLatestListingID(CHAR_A);
                std::cout << "listingID: " << listingID << "\n";  // 확인
                listingIDs.push_back(listingID);
                SafeReturn(regRes_raw);
            }
            std::cout << std::format("  {}개 listing 등록 완료\n", listingIDs.size());
        }

        auto snapshot_before = db.Snapshot();

        // buyer BUYER_COUNT개 동시 BUY
        std::vector<std::thread> threads;
        std::atomic<int> successCount{ 0 };
        std::atomic<int> failCount{ 0 };

        for (int i = 0; i < BUYER_COUNT; ++i) {
            threads.emplace_back([&, i]() {
                uint64_t charID = CHAR_LOAD_BASE + i; 
                uint64_t sessionID = SESSION_B + 100 + i;

                CacheNode buyer;
                WarmUp(buyer, CHAR_B, SESSION_B);

                Core::MsgDiamondDepositBody diamondDeposit{};
                diamondDeposit.characterID = charID;
                diamondDeposit.diamond = 10000;
                auto msg1 = MakeMsg(Core::MSG_DIAMOND_DEPOSIT, sessionID, diamondDeposit);
                buyer.Send(msg1);
                SafeReturn(msg1);
                SafeReturn(buyer.Wait());

                for (uint64_t lid : listingIDs) {
                    Core::MsgBazaarBuyBody buyBody{};
                    buyBody.characterID = charID;
                    buyBody.listingID = lid;
                    auto msg2 = MakeMsg(Core::MSG_BAZAAR_BUY, sessionID, buyBody);
                    buyer.Send(msg2);
                    SafeReturn(msg2);

                    auto* res = buyer.Wait();
                    if (!res) continue;
                    if (GetBody<Core::MsgBazaarBuyResBody>(res)->resStatus == 1)
                        ++successCount;
                    else
                        ++failCount;
                    SafeReturn(res);
                }
                });
        }
        for (auto& t : threads) t.join();

        auto snapshot_after = db.Snapshot();

        std::cout << std::format("  결과: 성공={} 실패={}\n", successCount.load(), failCount.load());
        std::cout << std::format("  기대: 성공={} 실패={}\n",
            LISTING_COUNT, BUYER_COUNT * LISTING_COUNT - LISTING_COUNT);
        db.PrintDiff(snapshot_before, snapshot_after);
    }

    static void WarmUp(CacheNode& node, uint64_t charID, uint64_t sessionID) {
        // 인벤토리
        Core::MsgInventoryReqBody invReq{};
        invReq.characterID = charID;
        auto msg1 = MakeMsg(Core::MSG_INVENTORY_REQ, sessionID, invReq);
        node.Send(msg1);
        SafeReturn(msg1);
        SafeReturn(node.Wait());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // currency
        Core::MsgCurrencyReqBody curReq{};
        curReq.characterID = charID;
        auto msg2 = MakeMsg(Core::MSG_CURRENCY_REQ, sessionID, curReq);
        node.Send(msg2);
        SafeReturn(msg2);
        SafeReturn(node.Wait());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // ----------------------------------------------------------------
    // 8. 부하 테스트 - 단일 listing 최대 lock 경합
    // ----------------------------------------------------------------
    inline void Test8_LockContention() {
        std::cout << "\n=== [8] 부하 테스트 - lock 경합 ===\n";

        // mysql max connection 기본값 151

        constexpr int SERVER_COUNT = 20;
        constexpr int BUYER_PER_SERVER = 2; // 서버당 동시 요청
        constexpr int TOTAL_BUYERS = SERVER_COUNT * BUYER_PER_SERVER; 

        DBChecker db;
        db.CleanupBazaar();

        // seller: listing 1개 등록
        uint64_t listingID = 0;
        {
            CacheNode seller;
            WarmUp(seller, CHAR_A, SESSION_A);

            Core::MsgInventoryUpdateBody addItem{};
            addItem.characterID = CHAR_A;
            addItem.itemID = 1;
            addItem.op = 1;
            addItem.change = 1;
            auto msg1 = MakeMsg(Core::MSG_INVENTORY_UPDATE, SESSION_A, addItem);
            seller.Send(msg1);
            SafeReturn(msg1);
            SafeReturn(seller.Wait());

            Core::MsgBazaarRegisterBody regBody{};
            regBody.characterID = CHAR_A;
            regBody.itemID = 1;
            regBody.quantity = 1;
            regBody.price = 50;
            auto msg2 = MakeMsg(Core::MSG_BAZAAR_REGISTER, SESSION_A, regBody);
            seller.Send(msg2);
            SafeReturn(msg2);
            auto* regRes_raw = seller.Wait();
            std::cout << "Registerd\n";
            SafeReturn(regRes_raw);
        }
		listingID = db.FetchLatestListingID(CHAR_A);
        // DB 스냅샷 + lock time 기록
        auto snapshot_before = db.Snapshot();
        auto lockTimeBefore = db.QuerySingle("Innodb_row_lock_time");

        std::vector<std::thread> threads;
        std::atomic<int> successCount{ 0 };
        std::atomic<int> failCount{ 0 };

        for (int s = 0; s < SERVER_COUNT; ++s) {
            for (int b = 0; b < BUYER_PER_SERVER; ++b) {
                threads.emplace_back([&, s, b]() {
                    int idx = s * BUYER_PER_SERVER + b;
                    uint64_t charID = CHAR_LOAD_BASE + idx; 
                    uint64_t sessionID = SESSION_B + 200 + idx;

                    CacheNode buyer;
                    WarmUp(buyer, charID, sessionID);

                    Core::MsgDiamondDepositBody diamondDeposit{};
                    diamondDeposit.characterID = charID;
                    diamondDeposit.diamond = 10000;
                    auto msg1 = MakeMsg(Core::MSG_DIAMOND_DEPOSIT, sessionID, diamondDeposit);
                    buyer.Send(msg1);
                    SafeReturn(msg1);
                    SafeReturn(buyer.Wait());

                    Core::MsgBazaarBuyBody buyBody{};
                    buyBody.characterID = charID;
                    buyBody.listingID = listingID;
                    auto msg2 = MakeMsg(Core::MSG_BAZAAR_BUY, sessionID, buyBody);
                    buyer.Send(msg2);
                    SafeReturn(msg2);

                    auto* res = buyer.Wait();
                    if (!res) return;
                    if (GetBody<Core::MsgBazaarBuyResBody>(res)->resStatus == 1)
                        ++successCount;
                    else
                        ++failCount;
                    SafeReturn(res);
                    });
            }
        }
        for (auto& t : threads) t.join();

        auto snapshot_after = db.Snapshot();
        auto lockTimeAfter = db.QuerySingle("Innodb_row_lock_time");
        auto lockWaitAvg = db.QuerySingle("Innodb_row_lock_time_avg");

        std::cout << std::format("  결과: 성공={} 실패={} (기대: 성공=1 실패={})\n",
            successCount.load(), failCount.load(), TOTAL_BUYERS - 1);
        db.PrintDiff(snapshot_before, snapshot_after);
        std::cout << std::format("  lock_time+={} ms  lock_time_avg={} ms\n",
            lockTimeAfter - lockTimeBefore, lockWaitAvg);
    }

    static void SafeReturn(Core::Message* msg) {
        if (msg) 
            g_msgPool().Return(msg);
    }
    // ================================================================
    // 전체 실행
    // ================================================================
    inline void RunAll() {
#if defined(TEST_CRASH) 
        Test6_Crash();
#elif defined(TEST_LOAD)
        Test8_LockContention();
#elif defined(TEST_RACE)
        Test7_Race();
#else
        Test1_Inventory();
        Test2_Currency();
        Test3_Diamond();
        Test4_BazaarBasic();
        Test5_SellerOffline();
#endif
    }

} // namespace Test