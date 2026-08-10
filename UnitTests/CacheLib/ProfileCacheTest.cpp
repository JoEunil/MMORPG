#include <gtest/gtest.h>

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

#include <CoreLib/IMessageQueue.h>
#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CacheLib/Initializer.h>
#include <CacheLib/ProfileCache.h>
#include <CacheLib/Config.h>

// Profile Cache 통합테스트
// 전제 조건: 로컬 DB(localhost, root/1234, game) 실행 중, Resources/DB/createCharacter.sql로 시딩됨.

namespace {
    class MockMessageQueue : public Core::IMessageQueue {
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::queue<Core::Message*> m_received;
    public:
        void EnqueueMessage(Core::Message* msg) override {
            std::lock_guard lock(m_mutex);
            m_received.push(msg);
            m_cv.notify_one();
        }
        Core::Message* WaitFor(int ms = 5000) {
            std::unique_lock lock(m_mutex);
            if (m_cv.wait_for(lock, std::chrono::milliseconds(ms), [this] { return !m_received.empty(); })) {
                auto* msg = m_received.front();
                m_received.pop();
                return msg;
            }
            return nullptr;
        }
    };

    template<typename BodyType>
    Core::Message* MakeMsg(uint16_t msgType, uint64_t sessionID, const BodyType& body) {
        auto* msg = new Core::Message(Cache::MESSAGE_LEN);
        auto* st = reinterpret_cast<Core::MsgStruct<BodyType>*>(msg->GetBuffer());
        st->header.messageType = msgType;
        st->header.sessionID = sessionID;
        st->body = body;
        msg->SetLength(sizeof(Core::MsgStruct<BodyType>));
        return msg;
    }

    void Settle() {
        // Unload/CleanUp 경로는 응답이 없어, worker가 처리할 시간을 준다.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

class ProfileCacheTest : public ::testing::Test {
protected:
    MockMessageQueue mockMQ;
    Cache::Initializer init;
    Core::IMessageQueue* recvMQ = nullptr;
    Cache::ProfileCache* cache = nullptr;
    uint64_t nextSessionID = 1;

    void SetUp() override {
        init.Initialize();
        init.InjectDependencies(&mockMQ);
        recvMQ = init.GetMessageQueue();
        cache = init.GetProfileCacheImpl();
    }

    // EnterWorld 흐름을 흉내내 DB에서 실제 profile_id/version/name을 읽어 캐시에 적재한다.
    Core::MsgCharacterStateResBody LoadChar(uint64_t charID) {
        Core::MsgCharacterStateReqBody body{};
        body.characterID = charID;
        recvMQ->EnqueueMessage(MakeMsg(Core::MSG_CHARACTER_STATE_REQ, nextSessionID++, body));
        auto* res = mockMQ.WaitFor();
        EXPECT_NE(res, nullptr);
        auto* rb = Core::parseMsgBody<Core::MsgCharacterStateResBody>(res->GetBuffer());
        EXPECT_EQ(rb->resStatus, 1);
        return *rb;
    }

    void UnloadProfile(uint32_t profileId) {
        Core::MsgCharacterStateUpdateBody body{};
        body.profileId = profileId;
        recvMQ->EnqueueMessage(MakeMsg(Core::MSG_CHARACTER_STATE_UPDATE, nextSessionID++, body));
        Settle();
    }

    Core::MsgProfileRenameResBody Rename(uint64_t sessionID, uint32_t profileId, const std::string& name) {
        Core::MsgProfileRenameBody body{};
        body.profileId = profileId;
        std::memset(body.name, 0, sizeof(body.name));
        size_t len = name.size() < sizeof(body.name) - 1 ? name.size() : sizeof(body.name) - 1;
        std::memcpy(body.name, name.data(), len);
        recvMQ->EnqueueMessage(MakeMsg(Core::MSG_PROFILE_RENAME, sessionID, body));
        auto* res = mockMQ.WaitFor();
        EXPECT_NE(res, nullptr);
        return *Core::parseMsgBody<Core::MsgProfileRenameResBody>(res->GetBuffer());
    }
};

// --- 적재 / 삭제 ---

TEST_F(ProfileCacheTest, LoadThenGetReturnsEntry) {
    auto st = LoadChar(41);

    Core::ProfileEntry out[1];
    uint16_t found = cache->GetBatch(&st.profileId, 1, out);

    ASSERT_EQ(found, 1);
    EXPECT_EQ(out[0].profileId, st.profileId);
    EXPECT_EQ(out[0].version, st.profileVersion);
    EXPECT_STREQ(reinterpret_cast<const char*>(out[0].name), reinterpret_cast<const char*>(st.name));
}

TEST_F(ProfileCacheTest, UnloadRemovesEntry) {
    auto st = LoadChar(42);
    UnloadProfile(st.profileId);

    Core::ProfileEntry out[1];
    uint16_t found = cache->GetBatch(&st.profileId, 1, out);

    EXPECT_EQ(found, 0);
}

TEST_F(ProfileCacheTest, LoadTwiceOverwritesWithLatestVersion) {
    cache->Load(1, 1, "old_name");
    cache->Load(1, 2, "new_name");

    uint32_t id = 1;
    Core::ProfileEntry out[1];
    uint16_t found = cache->GetBatch(&id, 1, out);

    ASSERT_EQ(found, 1);
    EXPECT_EQ(out[0].version, 2);
    EXPECT_STREQ(reinterpret_cast<const char*>(out[0].name), "new_name");
}

// --- 배치 조회 ---

TEST_F(ProfileCacheTest, GetBatchReturnsOnlyCachedIds) {
    auto a = LoadChar(41);
    auto b = LoadChar(42);
    uint32_t unloadedId = 999999999; // 캐시에 없는 임의의 id

    uint32_t ids[3] = { a.profileId, b.profileId, unloadedId };
    Core::ProfileEntry out[3];
    uint16_t found = cache->GetBatch(ids, 3, out);

    EXPECT_EQ(found, 2);
}

TEST_F(ProfileCacheTest, GetBatchWithNoCachedIdReturnsZero) {
    uint32_t id = 999999999;
    Core::ProfileEntry out[1];

    EXPECT_EQ(cache->GetBatch(&id, 1, out), 0);
}

TEST_F(ProfileCacheTest, RenameBumpsVersionAndUpdatesName) {
    auto st = LoadChar(43);
    auto res = Rename(nextSessionID++, st.profileId, "renamed_43");

    ASSERT_EQ(res.resStatus, 1);
    EXPECT_EQ(res.version, st.profileVersion + 1);

    Core::ProfileEntry out[1];
    cache->GetBatch(&st.profileId, 1, out);
    EXPECT_EQ(out[0].version, res.version);
    EXPECT_STREQ(reinterpret_cast<const char*>(out[0].name), "renamed_43");
}

TEST_F(ProfileCacheTest, RenamePersistsToDB) {
    auto st = LoadChar(44);
    auto res = Rename(nextSessionID++, st.profileId, "renamed_44");
    ASSERT_EQ(res.resStatus, 1);

    UnloadProfile(st.profileId); // 캐시에서 내려서 재조회가 DB를 거치게 만든다
    Core::MsgCharacterStateResBody afterReload = LoadChar(44);

    EXPECT_EQ(afterReload.profileVersion, res.version);
    EXPECT_STREQ(reinterpret_cast<const char*>(afterReload.name), "renamed_44");
}

TEST_F(ProfileCacheTest, RenameOnUnloadedProfile) {
    auto st = LoadChar(45);
    UnloadProfile(st.profileId);

    auto res = Rename(nextSessionID++, st.profileId, "should_not_apply");

    EXPECT_EQ(res.resStatus, 0);
}