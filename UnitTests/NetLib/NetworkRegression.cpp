#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <future>
#include <optional>
#include <thread>

#include <NetLibrary/ClientContext.h>
#include <NetLibrary/NetPacketFilter.h>
#include <NetLibrary/NetPerfCollector.h>
#include <NetLibrary/PingManager.h>
#include <NetLibrary/SessionManager.h>

// Component regression tests: receive buffers are injected without real socket I/O.
// The real Context/SessionManager are exercised with a recording dispatcher and fixed RTT.
// Lock tests target specific interleavings, not all possible server execution orders.
namespace Net {
    namespace {
        using View = std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter>;

        class RecordingDispatcher : public Core::IPacketDispatcher {
        public:
            size_t processed = 0;
            size_t pongs = 0;
            size_t disconnects = 0;
            bool retain = false;
            std::vector<View> retained;
            std::function<void()> onPong;
            std::function<void()> onDisconnect;

            void Process(View packet) override {
                ++processed;
                if (retain)
                    retained.push_back(std::move(packet));
            }
            void Disconnect(uint64_t) override {
                ++disconnects;
                if (onDisconnect)
                    onDisconnect();
            }
            uint8_t HealthCheck(uint64_t) override {
                return Core::MASK_EXIST | Core::MASK_NOT_CHEAT | Core::MASK_AUTHENTICATED;
            }
            uint64_t GetRTT(View, uint64_t) override {
                ++pongs;
                if (onPong)
                    onPong();
                return 7;
            }
            void Ping(uint64_t, uint64_t, uint64_t) override {}
        };

        std::vector<uint8_t> MakeClientPacket(uint8_t opcode, uint32_t wireLength, size_t actualLength,
            uint8_t flags = 0) {
            std::vector<uint8_t> bytes((std::max)(actualLength, sizeof(Core::PacketHeader)), 0);
            Core::PacketHeader header;
            header.length = wireLength;
            header.opcode = opcode;
            header.flags = flags;
            std::memcpy(bytes.data(), &header, sizeof(header));
            return bytes;
        }
    }

    class NetworkRegressionTest : public ::testing::Test {
    protected:
        static constexpr SOCKET socket = 4;
        static constexpr uint64_t session = 42;
        RecordingDispatcher dispatcher;
        SessionManager manager;
        NetPerfCollector collector;
        ClientContext context;
        Core::IPacketDispatcher* previousDispatcher;
        SessionManager* previousManager;
        NetPerfCollector* previousCollector;

        void SetUp() override {
            previousDispatcher = NetPacketFilter::packetDispatcher;
            previousManager = NetPacketFilter::sessionManager;
            previousCollector = NetPacketFilter::perfCollector;
            NetPacketFilter::Initialize(&dispatcher, &manager, &collector);
            context.Clear(session);
            auto& shard = manager.m_sessionShard[(socket >> 2) & SESSION_SHARD_MASK];
            shard.sessions[socket].state.InitializeSession(session);
            shard.sessions[socket].context = &context;
            manager.m_reverseShard[session & SESSION_SHARD_MASK].socketMap[session] = socket;
        }
        void TearDown() override {
            dispatcher.retained.clear();
            NetPacketFilter::Initialize(previousDispatcher, previousManager, previousCollector);
        }

        bool Receive(const std::vector<uint8_t>& bytes, uint64_t id = session) {
            size_t offset = 0;
            while (offset < bytes.size()) {
                uint8_t* buffer = nullptr;
                const uint16_t capacity = context.BeginRecvIO(id, buffer);
                if (capacity == 0 || buffer == nullptr)
                    return false;
                const size_t count = (std::min)(static_cast<size_t>(capacity), bytes.size() - offset);
                std::memcpy(buffer, bytes.data() + offset, count);
                const bool accepted = context.EnqueueRecvQ(id, buffer, count);
                context.CompleteRecvIO(id);
                if (!accepted)
                    return false;
                offset += count;
            }
            return true;
        }

        void Recycle() {
            context.Disconnect();
            ASSERT_TRUE(context.CanRecycle());
            context.Clear(session + 1);
            EXPECT_EQ(context.m_releaseIdx, 0);
            EXPECT_EQ(context.m_last_op, RELEASE);
            EXPECT_TRUE(std::all_of(context.m_releaseQ.begin(), context.m_releaseQ.end(),
                [](const auto& entry) { return entry == EMPTY_PAIR; }));
        }

        void SeedStaleReleaseSlot() {
            context.m_releaseQ[RELEASE_Q_SIZE - 1] = { 10, 20 };
        }

        bool ContextMutexAvailableFromOtherThread() {
            // 이 helper는 PONG 수신 스레드와 별개의 테스트 스레드에서만 호출한다.
            if (!context.m_mutex.try_lock())
                return false;
            context.m_mutex.unlock();
            return true;
        }

        std::mutex& ShardMutex() {
            return manager.m_sessionShard[(socket >> 2) & SESSION_SHARD_MASK].mutex;
        }

        uint64_t CurrentRTT() {
            return manager.m_sessionShard[(socket >> 2) & SESSION_SHARD_MASK].sessions.at(socket).state.GetRtt();
        }

        void ReuseSocketForNewSession() {
            manager.m_sessionShard[(socket >> 2) & SESSION_SHARD_MASK].sessions.at(socket).state.InitializeSession(session + 1);
        }

        void ReachPingLimit() {
            auto& state = manager.m_sessionShard[(socket >> 2) & SESSION_SHARD_MASK].sessions.at(socket).state;
            for (uint8_t i = 0; i < PING_COUNT_LIMIT; ++i)
                state.PingSent();
        }
    };

    TEST_F(NetworkRegressionTest, SessionLookupRejectsStaleAndMissingConnections) {
        ClientContext* owner = nullptr;
        uint64_t id = 0;
        uint8_t* buffer = nullptr;
        uint16_t length = 0;

        EXPECT_FALSE(manager.BeginRecvIO(socket, session + 1, owner, id, buffer, length));
        EXPECT_FALSE(manager.BeginRecvIO(socket + 4, session, owner, id, buffer, length));
        EXPECT_FALSE(manager.Disconnect(socket, session + 1));
        EXPECT_FALSE(manager.Disconnect(socket + 4, session));

        ASSERT_TRUE(manager.BeginRecvIO(socket, session, owner, id, buffer, length));
        EXPECT_EQ(owner, &context);
        EXPECT_EQ(id, session);
        EXPECT_NE(buffer, nullptr);
        EXPECT_GT(length, 0);
        context.CompleteRecvIO(session);
        EXPECT_TRUE(context.IsSessionAlive(session));
        EXPECT_EQ(manager.GetSocket(session), socket);
    }

    TEST_F(NetworkRegressionTest, BazaarSearchExactPacketDispatchesAfterFragmentedReceive) {
        auto bytes = MakeClientPacket(Core::OP::BAZAAR_SEARCH, 16, 16);
        *Core::parseBody<Core::BazaarSearchRequestBody>(bytes.data()) = {123, 3, 65535};
        EXPECT_TRUE(Receive(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 8)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_TRUE(Receive(std::vector<uint8_t>(bytes.begin() + 8, bytes.end())));
        EXPECT_EQ(dispatcher.processed, 1);
    }

    TEST_F(NetworkRegressionTest, BazaarSearchSimulationFlagRejectsBeforeDispatcher) {
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::BAZAAR_SEARCH, 16, 16, Core::FLAG_SIMULATION)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_FALSE(context.CheckGameSession());
    }

    TEST_F(NetworkRegressionTest, BazaarSearchShortBodyRejectsBeforeDispatcher) {
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::BAZAAR_SEARCH, 15, 15)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_FALSE(context.CheckGameSession());
    }

    TEST_F(NetworkRegressionTest, BazaarSearchTrailingByteRejectsBeforeDispatcher) {
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::BAZAAR_SEARCH, 17, 17)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_FALSE(context.CheckGameSession());
    }

    TEST_F(NetworkRegressionTest, BazaarMyListExactPacketDispatchesAfterFragmentedReceive) {
        auto bytes = MakeClientPacket(Core::OP::BAZAAR_MY_LIST, 12, 12);
        *Core::parseBody<Core::BazaarMyListRequestBody>(bytes.data()) = {123};
        EXPECT_TRUE(Receive(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 8)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_TRUE(Receive(std::vector<uint8_t>(bytes.begin() + 8, bytes.end())));
        EXPECT_EQ(dispatcher.processed, 1);
    }

    TEST_F(NetworkRegressionTest, BazaarMyListSimulationFlagRejectsBeforeDispatcher) {
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::BAZAAR_MY_LIST, 12, 12, Core::FLAG_SIMULATION)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_FALSE(context.CheckGameSession());
    }

    TEST_F(NetworkRegressionTest, BazaarMyListShortBodyRejectsBeforeDispatcher) {
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::BAZAAR_MY_LIST, 11, 11)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_FALSE(context.CheckGameSession());
    }

    TEST_F(NetworkRegressionTest, BazaarMyListTrailingByteRejectsBeforeDispatcher) {
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::BAZAAR_MY_LIST, 13, 13)));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_FALSE(context.CheckGameSession());
    }

    TEST_F(NetworkRegressionTest, RejectsZeroLengthPongWithoutLooping) {
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::PONG, 0, sizeof(Core::PacketHeader))));
        EXPECT_FALSE(context.CheckGameSession());
        EXPECT_EQ(dispatcher.pongs, 0);
    }

    TEST_F(NetworkRegressionTest, RejectsLengthBeforeUint16Narrowing) {
        // 이전 uint16_t 변환에서는 65552가 정상 PONG 크기인 16으로 바뀌었다.
        const auto size = sizeof(Core::PacketStruct<Core::Pong>);
        EXPECT_FALSE(Receive(MakeClientPacket(Core::OP::PONG, 65536 + static_cast<uint32_t>(size), size)));
        EXPECT_EQ(dispatcher.pongs, 0);
    }

    TEST_F(NetworkRegressionTest, WaitsForFragmentedValidBody) {
        const auto size = sizeof(Core::PacketStruct<Core::AuthRequestBody>);
        const auto bytes = MakeClientPacket(Core::OP::AUTH, static_cast<uint32_t>(size), size);
        EXPECT_TRUE(Receive(std::vector<uint8_t>(bytes.begin(), bytes.begin() + sizeof(Core::PacketHeader))));
        EXPECT_EQ(dispatcher.processed, 0);
        EXPECT_TRUE(Receive(std::vector<uint8_t>(bytes.begin() + sizeof(Core::PacketHeader), bytes.end())));
        EXPECT_EQ(dispatcher.processed, 1);
    }

    TEST_F(NetworkRegressionTest, ForwardsInventoryRequestToMockDispatcher) {
        EXPECT_TRUE(Receive(MakeClientPacket(Core::OP::INVENTORY_REQ, sizeof(Core::PacketHeader), sizeof(Core::PacketHeader))));
        EXPECT_EQ(dispatcher.processed, 1);
    }

    TEST_F(NetworkRegressionTest, ReusedContextCanReleaseMoreThanOneRingOfData) {
        dispatcher.retain = true;
        const auto packet = MakeClientPacket(Core::OP::CHARACTER_LIST, sizeof(Core::PacketHeader), sizeof(Core::PacketHeader));
        ASSERT_TRUE(Receive(packet));
        ASSERT_TRUE(Receive(packet));
        ASSERT_EQ(dispatcher.retained.size(), 2);
        dispatcher.retained[1].reset();
        dispatcher.retained[0].reset();
        dispatcher.retained.clear();
        dispatcher.retain = false;
        SeedStaleReleaseSlot();
        Recycle();

        for (size_t i = 0; i < RING_BUFFER_SIZE / sizeof(Core::PacketHeader) + 16; ++i)
            ASSERT_TRUE(Receive(packet, session + 1)) << "packet " << i;
        EXPECT_EQ(context.GetWorkingCnt(), 0);
        EXPECT_EQ(context.GetPendingIOCnt(), 0);
    }

    TEST_F(NetworkRegressionTest, PongReleasesContextMutexBeforeWaitingForShard) {
        // shard를 잡고 PONG 파싱을 진행시킨다. PONG 통지는 shard에서 대기하더라도
        // Context mutex를 반환해야 shard -> Context 호출 경로와 교착되지 않는다.
        std::promise<void> parsed;
        auto parsedFuture = parsed.get_future();
        dispatcher.onPong = [&] { parsed.set_value(); };
        std::unique_lock shardLock(ShardMutex());
        auto received = std::async(std::launch::async, [&] {
            const auto size = sizeof(Core::PacketStruct<Core::Pong>);
            return Receive(MakeClientPacket(Core::OP::PONG, static_cast<uint32_t>(size), size));
        });
        const auto status = parsedFuture.wait_for(std::chrono::seconds(2));
        EXPECT_EQ(status, std::future_status::ready);
        bool unlocked = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        if (status == std::future_status::ready) {
            while (!(unlocked = ContextMutexAvailableFromOtherThread())
                && std::chrono::steady_clock::now() < deadline)
                std::this_thread::yield();
        }
        // 실패하더라도 shard를 해제해서 비동기 작업과 fixture가 안전하게 종료된다.
        shardLock.unlock();
        EXPECT_TRUE(unlocked);
        ASSERT_EQ(received.wait_for(std::chrono::seconds(2)), std::future_status::ready);
        EXPECT_TRUE(received.get());
        EXPECT_EQ(CurrentRTT(), 7);
        EXPECT_EQ(context.GetPendingIOCnt(), 0);
    }

    TEST_F(NetworkRegressionTest, StalePongDoesNotUpdateReusedSocket) {
        ReuseSocketForNewSession();
        manager.PongReceived(session, 123);
        EXPECT_EQ(CurrentRTT(), 0);
    }

    TEST_F(NetworkRegressionTest, LastAllowedPongCanResetCountBeforeNextPingTick) {
        ReachPingLimit();
        const auto size = sizeof(Core::PacketStruct<Core::Pong>);
        EXPECT_TRUE(manager.UpdateFlood(socket, session, static_cast<uint16_t>(size)));
        EXPECT_TRUE(Receive(MakeClientPacket(Core::OP::PONG, static_cast<uint32_t>(size), size)));
        EXPECT_TRUE(manager.CheckSession(socket));
    }

    TEST_F(NetworkRegressionTest, PingSnapshotIncrementsCountAndReportsExpiredSession) {
        std::vector<PingStruct> snapshot;
        for (uint8_t i = 0; i < PING_COUNT_LIMIT; ++i) {
            manager.GetSessionSnapshot(static_cast<int>((socket >> 2) & SESSION_SHARD_MASK), snapshot);
            ASSERT_EQ(snapshot.size(), 1);
            EXPECT_TRUE(snapshot[0].isAlive);
        }
        manager.GetSessionSnapshot(static_cast<int>((socket >> 2) & SESSION_SHARD_MASK), snapshot);
        ASSERT_EQ(snapshot.size(), 1);
        EXPECT_FALSE(snapshot[0].isAlive);
    }

    TEST_F(NetworkRegressionTest, DisconnectNotificationPinsContextAndRunsOnce) {
        dispatcher.retain = true;
        ASSERT_TRUE(Receive(MakeClientPacket(Core::OP::CHARACTER_LIST, sizeof(Core::PacketHeader), sizeof(Core::PacketHeader))));
        ASSERT_EQ(dispatcher.retained.size(), 1);
        context.Disconnect();
        ASSERT_FALSE(context.CanRecycle());
        EXPECT_EQ(dispatcher.disconnects, 0);

        std::promise<void> notifying;
        auto notifyingFuture = notifying.get_future();
        std::promise<void> finishNotification;
        auto finishFuture = finishNotification.get_future();
        dispatcher.onDisconnect = [&] {
            notifying.set_value();
            finishFuture.wait();
        };
        auto released = std::async(std::launch::async, [&] { dispatcher.retained[0].reset(); });
        const auto status = notifyingFuture.wait_for(std::chrono::seconds(2));
        EXPECT_EQ(status, std::future_status::ready);
        EXPECT_FALSE(context.CanRecycle());
        finishNotification.set_value();
        ASSERT_EQ(released.wait_for(std::chrono::seconds(2)), std::future_status::ready);
        released.get();
        EXPECT_TRUE(context.CanRecycle());
        dispatcher.onDisconnect = {};
        context.Disconnect();
        EXPECT_EQ(dispatcher.disconnects, 1);
    }

    TEST_F(NetworkRegressionTest, PendingRecvKeepsDisconnectedContextAlive) {
        uint8_t* buffer = nullptr;
        ASSERT_GT(context.BeginRecvIO(session, buffer), 0);
        context.Disconnect();
        EXPECT_FALSE(context.CanRecycle());
        EXPECT_EQ(dispatcher.disconnects, 1);
        context.CompleteRecvIO(session);
        EXPECT_TRUE(context.CanRecycle());
    }

    TEST_F(NetworkRegressionTest, ParsesHeaderAndBodyAcrossRingBoundary) {
        const auto packet = MakeClientPacket(Core::OP::CHARACTER_LIST, sizeof(Core::PacketHeader), sizeof(Core::PacketHeader));
        for (size_t i = 0; i < RING_BUFFER_SIZE / sizeof(Core::PacketHeader) - 2; ++i)
            ASSERT_TRUE(Receive(packet));
        // 남은 16바이트 중 12바이트를 소비해 다음 헤더가 경계 4바이트 전부터 시작하게 한다.
        ASSERT_TRUE(Receive(MakeClientPacket(Core::OP::CHARACTER_LIST, 12, 12)));
        const auto size = sizeof(Core::PacketStruct<Core::Pong>);
        EXPECT_TRUE(Receive(MakeClientPacket(Core::OP::PONG, static_cast<uint32_t>(size), size)));
        EXPECT_EQ(dispatcher.pongs, 1);
        EXPECT_EQ(context.GetWorkingCnt(), 0);
    }

    TEST(SessionStateRegressionTest, UnansweredPingsReachDisconnectLimit) {
        SessionState state;
        state.InitializeSession(1);
        for (uint8_t i = 0; i < PING_COUNT_LIMIT; ++i) {
            ASSERT_TRUE(state.CheckSession());
            state.PingSent();
        }
        EXPECT_FALSE(state.CheckSession());
        for (size_t i = 0; i < 300; ++i)
            state.PingSent();
        EXPECT_FALSE(state.CheckSession());
        state.PongReceived(7);
        EXPECT_TRUE(state.CheckSession());
        EXPECT_EQ(state.GetRtt(), 7);
    }

    TEST(SessionStateRegressionTest, ReuseResetsReceiveSizeWindow) {
        SessionState state;
        state.InitializeSession(1);
        for (uint32_t i = 0; i < RECV_WINDOW - 1; ++i)
            state.BufferReceived(RECV_BUFFER_SIZE);
        state.InitializeSession(2);
        state.BufferReceived(sizeof(Core::PacketHeader));
        EXPECT_TRUE(state.CheckSession());
    }

    TEST(SessionStateRegressionTest, ReceiveSizeFailureStaysInvalid) {
        SessionState state;
        state.InitializeSession(1);
        for (uint32_t i = 0; i < RECV_WINDOW; ++i)
            state.BufferReceived(RECV_BUFFER_SIZE);
        ASSERT_FALSE(state.CheckSession());
        state.BufferReceived(sizeof(Core::PacketHeader));
        EXPECT_FALSE(state.CheckSession());
    }
}
