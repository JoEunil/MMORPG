#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include <CoreLib/ISessionAuth.h>
#include <CoreLib/IIOCP.h>
#include <CoreLib/IPacketPool.h>
#include <CoreLib/LobbyZone.h>
#include <CoreLib/MessagePool.h>
#include <CoreLib/MessageQueueHandler.h>
#include <CoreLib/NonZoneThreadPool.h>
#include <CoreLib/BroadcastThreadPool.h>
#include <CoreLib/PacketWriter.h>
#include <CoreLib/PacketTypes.h>
#include <CoreLib/StateManager.h>
#include <CoreLib/ChatThreadPool.h>
#include <CoreLib/WorldMap.h>
#include <CacheLib/BazaarCancelResult.h>

namespace Core {
    namespace {
        using namespace std::chrono_literals;

        // Owns the copied callback, never calls the real external auth service.
        class ControlledAuth final : public ISessionAuth {
            std::mutex mutex;
            std::condition_variable changed;
            std::vector<std::unique_ptr<SessionCallbackData>> requests;
            bool held = false;
            bool gateOpen = false;
            bool gateTimedOut = false;
        public:
            void CheckSession(SessionCallbackData* data) override {
                std::unique_lock lock(mutex);
                requests.emplace_back(data);
                changed.notify_all();
                if (held && !changed.wait_for(lock, 2s, [&] { return gateOpen; }))
                    gateTimedOut = true;
            }
            bool WaitFor(size_t count) {
                std::unique_lock lock(mutex);
                return changed.wait_for(lock, 2s, [&] { return requests.size() >= count; });
            }
            void Hold() { held = true; } // Before the worker starts.
            void Open() {
                std::lock_guard lock(mutex);
                gateOpen = true;
                changed.notify_all();
            }
            bool TimedOut() {
                std::lock_guard lock(mutex);
                return gateTimedOut;
            }
            const SessionCallbackData& Request(size_t index) const {
                return *requests.at(index); // Worker stopped before inspecting callbacks.
            }
            void Complete(size_t index, uint8_t status = RES_STATUS::SUCCESS) {
                std::unique_ptr<SessionCallbackData> request;
                {
                    std::lock_guard lock(mutex);
                    request = std::move(requests.at(index));
                }
                request->callbackFunc(request->sessionID, status, request->userID);
            }
        };

        class TestPacket final : public IPacket {
            std::array<uint8_t, 2048> bytes;
            uint32_t length = 0;
        public:
            TestPacket() { bytes.fill(0xCD); }
            uint8_t* GetBuffer() override { return bytes.data(); }
            uint32_t GetLength() const override { return length; }
            uint32_t GetCapacity() const override { return static_cast<uint32_t>(bytes.size()); }
            void SetLength(uint32_t value) override { length = value; }
            void Release() override { delete this; }
        };

        class TestPacketPool final : public IPacketPool {
        public:
            std::shared_ptr<IPacket> Acquire() override {
                return std::shared_ptr<IPacket>(new TestPacket, PacketDeleter{});
            }
            std::unique_ptr<IPacket, PacketDeleter> AcquireUnique() override {
                return std::unique_ptr<IPacket, PacketDeleter>(new TestPacket);
            }
            void Return(IPacket* packet) override { packet->Release(); }
        };

        class RecordingIOCP final : public IIOCP {
        public:
            std::vector<uint8_t> sentOpcodes;
            std::vector<uint8_t> authStatuses;
            std::vector<uint8_t> lastPacket;
            void SendData(uint64_t, std::shared_ptr<IPacket> packet) override {
                sentOpcodes.push_back(parseHeader(packet->GetBuffer())->opcode);
            }
            void SendDataChunks(uint64_t, std::shared_ptr<IPacket> packet,
                std::vector<std::shared_ptr<IPacket>>&) override {
                sentOpcodes.push_back(parseHeader(packet->GetBuffer())->opcode);
            }
            void SendDataUnique(uint64_t, std::unique_ptr<IPacket, PacketDeleter> packet) override {
                lastPacket.assign(packet->GetBuffer(), packet->GetBuffer() + packet->GetLength());
                const auto opcode = parseHeader(packet->GetBuffer())->opcode;
                sentOpcodes.push_back(opcode);
                if (opcode == OP::AUTH_RESPONSE)
                    authStatuses.push_back(parseBody<AuthResponseBody>(packet->GetBuffer())->resStatus);
            }
        };

        class ControlledLoadQueue final : public IMessageQueue {
            std::mutex mutex;
            std::condition_variable changed;
            std::unique_ptr<Message> pending;
        public:
            bool accept = true;
            bool HasRequest() { std::lock_guard lock(mutex); return pending != nullptr; }
            bool EnqueueMessage(Message* message) override {
                if (!accept) return false;
                auto copy = std::make_unique<Message>(message->GetLength());
                std::memcpy(copy->GetBuffer(), message->GetBuffer(), message->GetLength());
                copy->SetLength(message->GetLength());
                std::lock_guard lock(mutex);
                pending = std::move(copy);
                changed.notify_all();
                return true;
            }
            std::unique_ptr<Message> TakeRequest() {
                std::unique_lock lock(mutex);
                if (!changed.wait_for(lock, 2s, [&] { return pending != nullptr; }))
                    return nullptr;
                return std::move(pending);
            }
        };

        // The test retains storage so an erroneous second Release is countable.
        class CountedView final : public IPacketView {
            std::vector<uint8_t> packet;
            mutable std::mutex mutex;
            std::condition_variable released;
            size_t releaseCount = 0;
            uint64_t session;
        public:
            explicit CountedView(uint64_t id) : packet(sizeof(PacketStruct<AuthRequestBody>)), session(id) {
                auto* auth = reinterpret_cast<PacketStruct<AuthRequestBody>*>(packet.data());
                auth->header.length = static_cast<uint32_t>(packet.size());
                auth->header.opcode = OP::AUTH;
                auth->body.userID = 6982;
                std::memset(auth->body.sessionToken, 0x35, sizeof(auth->body.sessionToken));
            }
            CountedView(uint64_t id, uint64_t characterID)
                : packet(sizeof(PacketStruct<EnterWorldRequestBody>)), session(id) {
                auto* enter = reinterpret_cast<PacketStruct<EnterWorldRequestBody>*>(packet.data());
                enter->header.length = static_cast<uint32_t>(packet.size());
                enter->header.opcode = OP::ENTER_WORLD;
                enter->body.characterID = characterID;
            }
            CountedView(uint64_t id, const BazaarSearchRequestBody& request, uint8_t flags = 0, int sizeDelta = 0)
                : packet(sizeof(PacketStruct<BazaarSearchRequestBody>) + sizeDelta), session(id) {
                auto* header = parseHeader(packet.data());
                header->length = static_cast<uint32_t>(packet.size());
                header->opcode = OP::BAZAAR_SEARCH;
                header->flags = flags;
                if (sizeDelta >= 0)
                    *parseBody<BazaarSearchRequestBody>(packet.data()) = request;
            }
            CountedView(uint64_t id, const BazaarMyListRequestBody& request, uint8_t flags = 0, int sizeDelta = 0)
                : packet(sizeof(PacketStruct<BazaarMyListRequestBody>) + sizeDelta), session(id) {
                auto* header = parseHeader(packet.data());
                header->length = static_cast<uint32_t>(packet.size());
                header->opcode = OP::BAZAAR_MY_LIST;
                header->flags = flags;
                if (sizeDelta >= 0)
                    *parseBody<BazaarMyListRequestBody>(packet.data()) = request;
            }
            CountedView(uint64_t id, const BazaarClaimRequestBody& request, uint8_t flags = 0, int sizeDelta = 0)
                : packet(sizeof(PacketStruct<BazaarClaimRequestBody>) + sizeDelta), session(id) {
                auto* header = parseHeader(packet.data());
                header->magic = MAGIC;
                header->length = static_cast<uint32_t>(packet.size());
                header->opcode = OP::BAZAAR_CLAIM;
                header->flags = flags;
                if (sizeDelta >= 0)
                    *parseBody<BazaarClaimRequestBody>(packet.data()) = request;
            }
            CountedView(uint64_t id, const BazaarBuyRequestBody& request, uint8_t flags = 0, int sizeDelta = 0)
                : packet(sizeof(PacketStruct<BazaarBuyRequestBody>) + sizeDelta), session(id) {
                auto* header = parseHeader(packet.data());
                header->magic = MAGIC;
                header->length = static_cast<uint32_t>(packet.size());
                header->opcode = OP::BAZAAR_BUY;
                header->flags = flags;
                if (sizeDelta >= 0) *parseBody<BazaarBuyRequestBody>(packet.data()) = request;
            }
            CountedView(uint64_t id, const BazaarCheckOutboxRequestBody& request, uint8_t flags = 0, int sizeDelta = 0)
                : packet(sizeof(PacketStruct<BazaarCheckOutboxRequestBody>) + sizeDelta), session(id) {
                auto* header = parseHeader(packet.data());
                header->magic = MAGIC;
                header->length = static_cast<uint32_t>(packet.size());
                header->opcode = OP::BAZAAR_CHECK_OUTBOX;
                header->flags = flags;
                if (sizeDelta >= 0) *parseBody<BazaarCheckOutboxRequestBody>(packet.data()) = request;
            }
            CountedView(uint64_t id, const BazaarCancelListingRequestBody& request, uint8_t flags = 0, int sizeDelta = 0)
                : packet(sizeof(PacketStruct<BazaarCancelListingRequestBody>) + sizeDelta), session(id) {
                auto* header = parseHeader(packet.data());
                header->magic = MAGIC;
                header->length = static_cast<uint32_t>(packet.size());
                header->opcode = OP::BAZAAR_CANCEL_LISTING;
                header->flags = flags;
                if (sizeDelta >= 0) *parseBody<BazaarCancelListingRequestBody>(packet.data()) = request;
            }
            CountedView(uint64_t id, OP opcode)
                : packet(sizeof(PacketHeader)), session(id) {
                auto* header = parseHeader(packet.data());
                header->length = static_cast<uint32_t>(packet.size());
                header->opcode = static_cast<uint8_t>(opcode);
            }
            uint64_t GetSessionID() const override { return session; }
            uint8_t* GetPtr() const override {
                return const_cast<uint8_t*>(packet.data());
            }
            uint16_t GetLength() const override { return static_cast<uint16_t>(packet.size()); }
            uint8_t GetOpcode() const override { return parseHeader(GetPtr())->opcode; }
            void Release() override {
                std::lock_guard lock(mutex);
                ++releaseCount;
                released.notify_all();
            }
            bool WaitForRelease() {
                std::unique_lock lock(mutex);
                return released.wait_for(lock, 2s, [&] { return releaseCount != 0; });
            }
            size_t Releases() const {
                std::lock_guard lock(mutex);
                return releaseCount;
            }
            void OverwriteBody() {
                std::memset(packet.data() + sizeof(PacketHeader), 0, packet.size() - sizeof(PacketHeader));
            }
        };
    }

    class NonZoneThreadPoolFixture : public ::testing::Test {
    protected:
        // Queues and state contain large fixed arrays; keep them off the test stack.
        std::unique_ptr<StateManager> state = std::make_unique<StateManager>();
        std::unique_ptr<NonZoneThreadPool> pool = std::make_unique<NonZoneThreadPool>();
        NonZoneHandler handler;
        ControlledAuth auth;
        TestPacketPool packetPool;
        PacketWriter writer;
        BroadcastThreadPool broadcast;
        CorePerfCollector broadcastPerf;
        RecordingIOCP iocp;
        LobbyZone lobby;
        MessagePool messages;
        MessageQueueHandler responses;
        ControlledLoadQueue loads;
        std::vector<std::unique_ptr<CountedView>> views;
        // Component transitions enqueue into a stopped, value-initialized chat pool.
        // No chat worker or network service is started by these tests.
        std::unique_ptr<ChatThreadPool> transitionChat;

        void SetUp() override {
            broadcastPerf.Initialize();
            state->lobbyZone = &lobby; // Only the zone0 dependency; no Zone/topology initialization.
            lobby.Initialize(state.get());
            writer.Initialize(&packetPool, &packetPool, &packetPool);
            responses.Initialize(&iocp, &writer, &lobby, &messages);
            responses.InitializeStateManager(state.get());
            handler.Initialize(&iocp, &auth, &writer, &messages, &loads, state.get(), &lobby, nullptr);
            pool->Initialize(&handler);
        }
        void StartWorker() {
            pool->m_running.store(true, std::memory_order_relaxed);
            // Use the production WorkFunc, with one consumer for deterministic drain order.
            pool->m_threads.emplace_back(&NonZoneThreadPool::WorkFunc, pool.get());
        }
        void StopWorker() { pool->Stop(); }
        bool WaitForStopStarted() {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (pool->m_running.load(std::memory_order_relaxed)
                && std::chrono::steady_clock::now() < deadline)
                std::this_thread::yield();
            return !pool->m_running.load(std::memory_order_relaxed);
        }
        void TearDown() override {
            auth.Open();
            StopWorker();
            broadcast.Stop();
        }
        BroadcastThreadPool::BroadcastWorkPtr PopBroadcastWork() {
            BroadcastThreadPool::BroadcastWorkPtr work;
            EXPECT_TRUE(broadcast.m_workQ.pop(work));
            return work;
        }
        CountedView& Enqueue(uint64_t id = 42, uint64_t userID = 6982) {
            views.push_back(std::make_unique<CountedView>(id));
            auto& view = *views.back();
            parseBody<AuthRequestBody>(view.GetPtr())->userID = userID;
            pool->EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter>(&view));
            return view;
        }
        CountedView& EnqueueEnter(uint64_t id, uint64_t characterID) {
            views.push_back(std::make_unique<CountedView>(id, characterID));
            auto& view = *views.back();
            pool->EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter>(&view));
            return view;
        }
        bool HasLobbyCharacter(uint64_t sessionID) {
            std::lock_guard lock(lobby.m_mutex);
            return lobby.m_chars.contains(sessionID);
        }
        void ResetZones() { state->m_states.clear(); }
        void PrepareZones() {
            broadcast.Initialize(&iocp, state.get(), &broadcastPerf, &writer);
            broadcast.m_running.store(true, std::memory_order_relaxed);
            ZoneState::Initialize(&broadcast, &writer, state.get(), &broadcastPerf);
            state->iocp = &iocp;
            transitionChat = std::make_unique<ChatThreadPool>();
            state->chat = transitionChat.get();
            state->mq = &loads;
            for (uint16_t id = 1; id <= ZONE_COUNT; ++id)
                state->m_states.emplace(id, std::make_unique<ZoneState>(id));
        }
        bool LoadTransitionCharacter(uint64_t session = 42, uint16_t zone = 1,
            float x = ZONE_SIZE, float y = ZONE_SIZE * 0.5f, int hp = 75) {
            state->BeginAuth(session);
            if (!state->CompleteAuth(session, 6982, true)) return false;
            CharacterState character{};
            character.sessionID = session;
            character.characterID = 5000 + session;
            character.hp = hp;
            character.mp = 61;
            character.lastZone = zone;
            character.x = x;
            character.y = y;
            character.skillSlot = {{0, 17}, {1, 21}, {2, 0}};
            character.skillSlotCnt = 3;
            return lobby.ImmigrateChar(session, character);
        }
        CharacterState CharacterIn(uint16_t zone, uint64_t session = 42) {
            auto* z = state->GetZone(zone);
            std::lock_guard lock(z->m_mutex);
            return z->m_chars.at(z->m_sessionToIndex.at(session));
        }
        CharacterState CharacterInLobby(uint64_t session = 42) {
            std::lock_guard lock(lobby.m_mutex);
            return lobby.m_chars.at(session);
        }
        void VerifyTownAndHuntingSpawns() {
            auto* town = state->GetZone(TOWN_ZONE_ID);
            std::lock_guard townLock(town->m_mutex);
            EXPECT_TRUE(town->m_monsters.empty());
            for (const auto& row : town->m_cells)
                for (const auto& cell : row)
                    EXPECT_TRUE(cell.monsterIndexes.empty());
            auto* hunt = state->GetZone(2);
            std::lock_guard huntLock(hunt->m_mutex);
            ASSERT_EQ(hunt->m_monsters.size(), 112u);
            EXPECT_EQ(std::count_if(hunt->m_monsters.begin(), hunt->m_monsters.end(),
                [](const MonsterState& monster) { return monster.data->id == 2; }), 1);
        }
        template<class Update>
        void UpdateCharacter(uint16_t zone, uint64_t session, Update update) {
            auto* z = state->GetZone(zone);
            std::lock_guard lock(z->m_mutex);
            update(z->m_chars.at(z->m_sessionToIndex.at(session)));
        }
        uint64_t Generation(uint16_t zone) {
            auto* z = state->GetZone(zone);
            std::lock_guard lock(z->m_mutex);
            return z->m_generation;
        }
        uint64_t OwnershipEpoch(uint64_t sessionID = 42) {
            auto& shard = state->m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock lock(shard.smutex);
            return shard.sessionMap.at(sessionID).ownershipEpoch;
        }
        uint64_t BootstrapSequence(uint64_t sessionID = 42) {
            auto& shard = state->m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock lock(shard.smutex);
            return shard.sessionMap.at(sessionID).bootstrapSequence;
        }
        size_t Characters(uint16_t zone) {
            auto* z = state->GetZone(zone);
            std::lock_guard lock(z->m_mutex);
            return z->m_chars.size();
        }
        void SetPosition(uint16_t zone, float x, float y, uint64_t session = 42) {
            auto* z = state->GetZone(zone);
            std::lock_guard lock(z->m_mutex);
            auto& character = z->m_chars.at(z->m_sessionToIndex.at(session));
            character.x = x;
            character.y = y;
        }
        void SetMoveBudget(uint16_t zone, float budget, uint64_t session = 42) {
            auto* z = state->GetZone(zone);
            std::lock_guard lock(z->m_mutex);
            z->m_chars.at(z->m_sessionToIndex.at(session)).moveBudget = budget;
        }
        void ExhaustDestination(uint16_t zone, int kind) {
            auto* z = state->GetZone(zone);
            std::lock_guard lock(z->m_mutex);
            if (kind == 0) z->m_chars.resize(MAX_ZONE_CAPACITY);
            if (kind == 1) z->m_internalIdGenerator.store(MAX_ZONE_INTERNAL_ID);
            if (kind == 2) z->m_generation = (std::numeric_limits<uint64_t>::max)();
        }
        void AddOldCast() {
            auto* z = state->GetZone(1);
            std::lock_guard lock(z->m_mutex);
            const auto& character = z->m_chars.at(z->m_sessionToIndex.at(42));
            auto& cell = z->m_cells[character.cellY][character.cellX];
            ActiveSkill own{};
            own.sessionId = 42;
            own.zoneInternalId = character.zoneInternalID;
            cell.activeSkills.push_back(own);
            ActiveSkill monster{};
            monster.casterType = 1;
            cell.activeSkills.push_back(monster);
            ActionResult action{};
            action.zoneInternalId = character.zoneInternalID;
            cell.actionResults.push_back(action);
            cell.dirtyChar.push_back(character.zoneInternalID);
        }
        bool OldCastRemoved() {
            auto* z = state->GetZone(1);
            std::lock_guard lock(z->m_mutex);
            size_t monsters = 0;
            for (auto& row : z->m_cells) for (auto& cell : row) {
                if (!cell.actionResults.empty() || !cell.dirtyChar.empty()) return false;
                for (auto& skill : cell.activeSkills) {
                    if (skill.casterType == 0) return false;
                    ++monsters;
                }
            }
            return monsters == 1;
        }
        void RaceCleanup() {
            state->m_running.store(true);
            state->CleanUp();
        }
        bool FlushReleasesZoneBeforeWaitingForShard() {
            auto* zone = state->GetZone(1);
            auto* previousManager = ZoneState::stateManager;
            ZoneState::stateManager = state.get();
            zone->m_cheatList.emplace_back(42, 1);
            auto& shard = state->m_shards[42 & SHARD_SIZE_MASK];
            std::unique_lock shardLock(shard.smutex);
            auto flush = std::async(std::launch::async, [zone] { zone->FlushCheat(); });
            bool released = false;
            const auto deadline = std::chrono::steady_clock::now() + 2s;
            while (std::chrono::steady_clock::now() < deadline) {
                if (zone->m_mutex.try_lock()) {
                    // Empty means FlushCheat reached its swap, rather than this thread winning first.
                    released = zone->m_cheatList.empty();
                    zone->m_mutex.unlock();
                    if (released) break;
                }
                std::this_thread::yield();
            }
            shardLock.unlock();
            flush.get();
            ZoneState::stateManager = previousManager;
            return released;
        }
        bool IsBoundTo(uint64_t sessionID, uint64_t characterID) {
            return state->GetCharacterID(sessionID) == characterID;
        }
        void SetSessionImage(SessionData image) {
            auto& shard = state->m_shards[42 & SHARD_SIZE_MASK];
            std::unique_lock lock(shard.smutex);
            shard.sessionMap[42] = image;
        }
        void FinishSearch(uint32_t requestID) {
            auto* response = messages.Acquire();
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarSearchResBody>*>(response->GetBuffer());
            *st = {};
            st->header = { 42, MSG_BAZAAR_SEARCH_RES };
            st->body.requestID = requestID;
            st->body.resStatus = 1;
            response->SetLength(sizeof(*st));
            responses.Process(response);
        }
        void FinishMyList(uint32_t requestID, uint64_t characterID, int sizeDelta = 0) {
            auto* response = messages.Acquire();
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarMyListResBody>*>(response->GetBuffer());
            *st = {};
            st->header = { 42, MSG_BAZAAR_MY_LIST_RES };
            st->body.requestID = requestID;
            st->body.resStatus = 1;
            st->body.characterID = characterID;
            response->SetLength(sizeof(*st) + sizeDelta);
            responses.Process(response);
        }
        void FinishClaim(const MsgBazaarClaimResBody& body, int sizeDelta = 0) {
            auto* response = messages.Acquire();
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarClaimResBody>*>(response->GetBuffer());
            st->header = { 42, MSG_BAZAAR_CLAIM_RES };
            st->body = body;
            response->SetLength(sizeof(*st) + sizeDelta);
            responses.Process(response);
        }
        void FinishBuy(const MsgBazaarBuyResBody& body, int sizeDelta = 0) {
            auto* response = messages.Acquire();
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarBuyResBody>*>(response->GetBuffer());
            st->header = { 42, MSG_BAZAAR_BUY_RES }; st->body = body;
            response->SetLength(sizeof(*st) + sizeDelta); responses.Process(response);
        }
        void FinishOutbox(const MsgBazaarCheckOutboxResBody& body, int sizeDelta = 0) {
            auto* response = messages.Acquire();
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarCheckOutboxResBody>*>(response->GetBuffer());
            st->header = { 42, MSG_BAZAAR_CHECK_OUTBOX_RES }; st->body = body;
            response->SetLength(sizeof(*st) + sizeDelta); responses.Process(response);
        }
        void FinishCancelListing(const MsgBazaarCancelListingResBody& body, int sizeDelta = 0) {
            auto* response = messages.Acquire();
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarCancelListingResBody>*>(response->GetBuffer());
            st->header = { 42, MSG_BAZAAR_CANCEL_LISTING_RES }; st->body = body;
            response->SetLength(sizeof(*st) + sizeDelta); responses.Process(response);
        }
        bool CompleteLoadSuccessfully(Message& request) {
            auto* response = messages.Acquire();
            if (response == nullptr)
                return false;
            auto* result = reinterpret_cast<MsgStruct<MsgCharacterStateResBody>*>(response->GetBuffer());
            *result = {};
            result->header.sessionID = parseMsgHeader(request.GetBuffer())->sessionID;
            result->header.messageType = MSG_CHARACTER_STATE_RES;
            result->body.resStatus = RES_STATUS::SUCCESS;
            result->body.charID = parseMsgBody<MsgCharacterStateReqBody>(request.GetBuffer())->characterID;
            result->body.hp = result->body.maxHp = 100;
            result->body.mp = result->body.maxMp = 100;
            result->body.currentZone = 1;
            response->SetLength(sizeof(*result));
            responses.Process(response); // Real response handler returns the pooled Message.
            return true;
        }
    };

    TEST_F(NonZoneThreadPoolFixture, ReturnsLastPacketWithoutAnotherEnqueue) {
        StartWorker();
        auto& packet = Enqueue();
        ASSERT_TRUE(auth.WaitFor(1));
        // No later event or Stop is issued until the bounded Release observation.
        EXPECT_TRUE(packet.WaitForRelease());
        EXPECT_EQ(packet.Releases(), 1);
        StopWorker();
        StopWorker();
        EXPECT_EQ(packet.Releases(), 1);
    }

    TEST_F(NonZoneThreadPoolFixture, StopDrainsQueuedPacketsExactlyOnce) {
        auth.Hold();
        StartWorker();
        auto& first = Enqueue(42);
        ASSERT_TRUE(auth.WaitFor(1));
        auto& second = Enqueue(43);
        auto& third = Enqueue(44);
        // Production Stop closes the queue while the first handler is held.
        auto stopped = std::async(std::launch::async, [&] { StopWorker(); });
        EXPECT_TRUE(WaitForStopStarted());
        auth.Open();
        ASSERT_EQ(stopped.wait_for(std::chrono::seconds(2)), std::future_status::ready);
        stopped.get();
        EXPECT_FALSE(auth.TimedOut());
        EXPECT_EQ(first.Releases(), 1);
        EXPECT_EQ(second.Releases(), 1);
        EXPECT_EQ(third.Releases(), 1);
        EXPECT_EQ(auth.Request(0).sessionID, 42);
        EXPECT_EQ(auth.Request(1).sessionID, 43);
        EXPECT_EQ(auth.Request(2).sessionID, 44);
        StopWorker();
        EXPECT_EQ(third.Releases(), 1);
    }

    TEST_F(NonZoneThreadPoolFixture, PendingAuthOwnsCopiedRequestAfterPacketReturn) {
        StartWorker();
        auto& packet = Enqueue();
        ASSERT_TRUE(auth.WaitFor(1));
        ASSERT_TRUE(packet.WaitForRelease());
        packet.OverwriteBody();
        pool->EnqueueDisconnect(42); // The pending entry must be removed before completion.
        StopWorker(); // Join establishes that the real Disconnect event completed.
        EXPECT_EQ(state->GetZoneID(42), -1);
        EXPECT_EQ(packet.Releases(), 1);
        EXPECT_EQ(auth.Request(0).sessionID, 42);
        EXPECT_EQ(auth.Request(0).userID, 6982);
        EXPECT_EQ(auth.Request(0).sessionToken.front(), 0x35);
        EXPECT_EQ(auth.Request(0).sessionToken.back(), 0x35);
        EXPECT_TRUE(static_cast<bool>(auth.Request(0).callbackFunc));
        // Separate late-completion tests execute the saved callback.
    }

    TEST_F(NonZoneThreadPoolFixture, LateAuthSuccessDoesNotRecreateDisconnectedSession) {
        StartWorker();
        auto& packet = Enqueue(42);
        ASSERT_TRUE(auth.WaitFor(1));
        ASSERT_TRUE(packet.WaitForRelease());
        pool->EnqueueDisconnect(42);
        StopWorker(); // Production Stop joins the worker after its queued Disconnect.
        ASSERT_EQ(state->GetZoneID(42), -1);
        ASSERT_FALSE(HasLobbyCharacter(42));

        auth.Complete(0); // Real ResponseSession, after disconnect completion.

        EXPECT_EQ(state->GetZoneID(42), -1);
        EXPECT_EQ(state->GetUserID(42), 0);
        EXPECT_FALSE(HasLobbyCharacter(42));
        EXPECT_TRUE(iocp.sentOpcodes.empty());
        EXPECT_EQ(packet.Releases(), 1);
    }

    TEST_F(NonZoneThreadPoolFixture, LateEnterSuccessDoesNotRecreateDisconnectedLobbyCharacter) {
        StartWorker();
        auto& authPacket = Enqueue(42);
        ASSERT_TRUE(auth.WaitFor(1));
        ASSERT_TRUE(authPacket.WaitForRelease());
        auth.Complete(0); // Establish an authenticated session through the real callback.
        ASSERT_EQ(state->GetUserID(42), 6982);
        ASSERT_EQ(state->GetZoneID(42), 0);
        ASSERT_EQ(iocp.sentOpcodes.size(), 1);
        ASSERT_EQ(iocp.sentOpcodes.front(), OP::AUTH_RESPONSE);

        auto& enterPacket = EnqueueEnter(42, 5012);
        auto request = loads.TakeRequest();
        ASSERT_NE(request, nullptr);
        ASSERT_EQ(parseMsgHeader(request->GetBuffer())->messageType, MSG_CHARACTER_STATE_REQ);
        ASSERT_EQ(parseMsgBody<MsgCharacterStateReqBody>(request->GetBuffer())->userID, 6982);
        ASSERT_EQ(parseMsgBody<MsgCharacterStateReqBody>(request->GetBuffer())->characterID, 5012);
        ASSERT_TRUE(enterPacket.WaitForRelease());
        pool->EnqueueDisconnect(42);
        StopWorker();
        ASSERT_EQ(state->GetZoneID(42), -1);
        ASSERT_FALSE(HasLobbyCharacter(42));

        ASSERT_TRUE(CompleteLoadSuccessfully(*request)); // Real MessageQueueHandler after close.

        EXPECT_EQ(state->GetZoneID(42), -1);
        EXPECT_EQ(state->GetUserID(42), 0);
        EXPECT_FALSE(HasLobbyCharacter(42));
        EXPECT_EQ(iocp.sentOpcodes.size(), 1); // Rejected late load sends no ENTER response.
        EXPECT_EQ(enterPacket.Releases(), 1);
    }

    TEST_F(NonZoneThreadPoolFixture, PendingAuthAndFirstSuccessSurviveLaterCallbacksUntilClose) {
        StartWorker();
        auto& first = Enqueue(42, 6982);
        auto& second = Enqueue(42, 9999);
        auto& third = Enqueue(42, 1111);
        ASSERT_TRUE(auth.WaitFor(3));
        ASSERT_TRUE(first.WaitForRelease());
        ASSERT_TRUE(second.WaitForRelease());
        ASSERT_TRUE(third.WaitForRelease());
        EXPECT_EQ(state->GetUserID(42), 0);
        EXPECT_EQ(state->HealthCheck(42), MASK_EXIST | MASK_NOT_CHEAT);

        auth.Complete(0);
        EXPECT_EQ(state->GetUserID(42), 6982);
        EXPECT_EQ(state->HealthCheck(42), MASK_EXIST | MASK_NOT_CHEAT | MASK_AUTHENTICATED);
        auth.Complete(1, RES_STATUS::FAILED);
        auth.Complete(2);
        EXPECT_EQ(state->GetUserID(42), 6982);
        EXPECT_EQ(state->HealthCheck(42), MASK_EXIST | MASK_NOT_CHEAT | MASK_AUTHENTICATED);
        EXPECT_EQ(iocp.authStatuses, (std::vector<uint8_t>{ RES_STATUS::SUCCESS,
            RES_STATUS::FAILED, RES_STATUS::SUCCESS }));

        pool->EnqueueDisconnect(42);
        StopWorker();
        EXPECT_EQ(state->HealthCheck(42), 0);
        EXPECT_FALSE(HasLobbyCharacter(42));
    }

    TEST_F(NonZoneThreadPoolFixture, FirstFailurePreservesStateAndLaterCallbackResponseStatuses) {
        StartWorker();
        auto& firstRequest = Enqueue(42, 6982);
        auto& secondRequest = Enqueue(42, 9999);
        auto& thirdRequest = Enqueue(42, 1111);
        ASSERT_TRUE(auth.WaitFor(3));
        ASSERT_TRUE(firstRequest.WaitForRelease());
        ASSERT_TRUE(secondRequest.WaitForRelease());
        ASSERT_TRUE(thirdRequest.WaitForRelease());

        auth.Complete(1, RES_STATUS::FAILED); // Completion order, not request order, selects state.
        EXPECT_EQ(state->GetUserID(42), 9999);
        EXPECT_EQ(state->HealthCheck(42), MASK_EXIST | MASK_NOT_CHEAT);
        ASSERT_EQ(iocp.authStatuses, (std::vector<uint8_t>{ RES_STATUS::FAILED }));
        auth.Complete(0);
        auth.Complete(2, RES_STATUS::FAILED);
        EXPECT_EQ(state->GetUserID(42), 9999);
        EXPECT_EQ(state->HealthCheck(42), MASK_EXIST | MASK_NOT_CHEAT);
        EXPECT_EQ(iocp.authStatuses, (std::vector<uint8_t>{ RES_STATUS::FAILED,
            RES_STATUS::SUCCESS, RES_STATUS::FAILED }));

        pool->EnqueueDisconnect(42);
        StopWorker();
        EXPECT_EQ(state->HealthCheck(42), 0);
    }

    TEST_F(NonZoneThreadPoolFixture, ZeroUserIDIsACompletedFirstAuthResult) {
        StartWorker();
        auto& zeroUser = Enqueue(42, 0);
        auto& otherUser = Enqueue(42, 9999);
        ASSERT_TRUE(auth.WaitFor(2));
        ASSERT_TRUE(zeroUser.WaitForRelease());
        ASSERT_TRUE(otherUser.WaitForRelease());
        auth.Complete(0);
        EXPECT_EQ(state->GetZoneID(42), 0);
        EXPECT_EQ(state->HealthCheck(42), MASK_EXIST | MASK_NOT_CHEAT | MASK_AUTHENTICATED);
        auth.Complete(1);
        EXPECT_EQ(state->GetUserID(42), 0);
        EXPECT_EQ(state->HealthCheck(42), MASK_EXIST | MASK_NOT_CHEAT | MASK_AUTHENTICATED);
        EXPECT_EQ(iocp.authStatuses, (std::vector<uint8_t>{ RES_STATUS::SUCCESS, RES_STATUS::SUCCESS }));
        pool->EnqueueDisconnect(42);
        StopWorker();
        EXPECT_EQ(state->GetZoneID(42), -1);
    }

    TEST_F(NonZoneThreadPoolFixture, LoadedLobbyCloseAndRollbackComponentBoundaries) {
        StartWorker();
        auto& authPacket = Enqueue(42);
        ASSERT_TRUE(auth.WaitFor(1));
        ASSERT_TRUE(authPacket.WaitForRelease());
        auth.Complete(0);
        auto& enterPacket = EnqueueEnter(42, 5012);
        auto request = loads.TakeRequest();
        ASSERT_NE(request, nullptr);
        ASSERT_EQ(parseMsgHeader(request->GetBuffer())->messageType, MSG_CHARACTER_STATE_REQ);
        ASSERT_EQ(parseMsgBody<MsgCharacterStateReqBody>(request->GetBuffer())->characterID, 5012);
        ASSERT_TRUE(enterPacket.WaitForRelease());
        ASSERT_TRUE(CompleteLoadSuccessfully(*request)); // Load completion before close.
        ASSERT_TRUE(HasLobbyCharacter(42));
        ASSERT_EQ(state->GetCharacterID(42), 5012);
        ASSERT_EQ(iocp.sentOpcodes.size(), 2);
        EXPECT_EQ(iocp.sentOpcodes.back(), OP::ENTER_WORLD_RESPONSE);

        // Component rollback only: no ZoneChange/ZoneState immigration is executed.
        CharacterState snapshot{};
        snapshot.characterID = 9001;
        EXPECT_FALSE(lobby.ImmigrateChar(42, snapshot)); // Existing lobby entry rejects duplicates.
        EXPECT_EQ(state->GetCharacterID(42), 5012); // Duplicate entry cannot replace the bound character.
        ASSERT_TRUE(lobby.EmigrateChar(42, snapshot));
        EXPECT_EQ(snapshot.characterID, 5012);
        ASSERT_TRUE(lobby.ImmigrateChar(42, snapshot)); // Authenticated zone0 rollback remains valid.
        pool->EnqueueDisconnect(42);
        StopWorker();
        EXPECT_EQ(state->GetZoneID(42), -1);
        EXPECT_FALSE(HasLobbyCharacter(42));
        EXPECT_FALSE(lobby.ImmigrateChar(42, snapshot)); // Closed session cannot be rolled back.
        EXPECT_EQ(state->GetCharacterID(42), 0);
    }

    TEST_F(NonZoneThreadPoolFixture, LobbyAdmissionRejectsPendingFailedAndNonLobbySessionsComponent) {
        StartWorker();
        auto& pending = Enqueue(42);
        auto& authenticated = Enqueue(43);
        ASSERT_TRUE(auth.WaitFor(2));
        ASSERT_TRUE(pending.WaitForRelease());
        ASSERT_TRUE(authenticated.WaitForRelease());
        CharacterState snapshot{};
        snapshot.characterID = 5012;
        EXPECT_FALSE(lobby.ImmigrateChar(42, snapshot));
        auth.Complete(0, RES_STATUS::FAILED);
        EXPECT_FALSE(lobby.ImmigrateChar(42, snapshot));
        auth.Complete(1);
        // Public state transition for this component guard only; no actual Zone is started.
        state->SetZoneID(43, 1);
        EXPECT_FALSE(lobby.ImmigrateChar(43, snapshot));
        state->SetZoneID(43, 0);
        pool->EnqueueDisconnect(42);
        pool->EnqueueDisconnect(43);
        StopWorker();
        EXPECT_FALSE(HasLobbyCharacter(42));
        EXPECT_FALSE(HasLobbyCharacter(43));
        EXPECT_EQ(state->GetCharacterID(42), 0);
        EXPECT_EQ(state->GetCharacterID(43), 0);
    }

    TEST_F(NonZoneThreadPoolFixture, InventoryResponseRejectsFailureAndOversizedCountWithoutLeakingBuffer) {
        MsgInventoryResBody failed;
        std::memset(&failed, 0xA5, sizeof(failed));
        failed.resStatus = RES_STATUS::FAILED;
        failed.itemCount = MAX_INVENTORY_ITEMS;

        auto failedPacket = writer.WriteInventoryResponse(&failed);
        ASSERT_NE(failedPacket, nullptr);
        ASSERT_EQ(failedPacket->GetLength(), sizeof(PacketStruct<InventoryResBody>));
        const auto* failedResponse = reinterpret_cast<const PacketStruct<InventoryResBody>*>(failedPacket->GetBuffer());
        EXPECT_EQ(failedResponse->body.resStatus, RES_STATUS::FAILED);
        EXPECT_EQ(failedResponse->body.itemCount, 0);
        for (const auto& item : failedResponse->body.items) {
            EXPECT_EQ(item.itemID, 0);
            EXPECT_EQ(item.quantity, 0);
            EXPECT_EQ(item.slot, 0);
        }

        MsgInventoryResBody oversized{};
        oversized.resStatus = RES_STATUS::SUCCESS;
        oversized.itemCount = MAX_INVENTORY_ITEMS + 1;
        auto oversizedPacket = writer.WriteInventoryResponse(&oversized);
        ASSERT_NE(oversizedPacket, nullptr);
        const auto* oversizedResponse = reinterpret_cast<const PacketStruct<InventoryResBody>*>(oversizedPacket->GetBuffer());
        EXPECT_EQ(oversizedResponse->body.resStatus, RES_STATUS::FAILED);
        EXPECT_EQ(oversizedResponse->body.itemCount, 0);
        for (const auto& item : oversizedResponse->body.items) {
            EXPECT_EQ(item.itemID, 0);
            EXPECT_EQ(item.quantity, 0);
            EXPECT_EQ(item.slot, 0);
        }
    }

    TEST_F(NonZoneThreadPoolFixture, InventoryResponseCopiesMaximumCountAndZeroesUnusedTail) {
        MsgInventoryResBody body{};
        body.resStatus = RES_STATUS::SUCCESS;
        body.itemCount = MAX_INVENTORY_ITEMS;
        for (uint16_t i = 0; i < body.itemCount; ++i) {
            body.items[i].itemID = 100 + i;
            body.items[i].quantity = i + 1;
            body.items[i].slot = static_cast<uint8_t>(i);
        }

        auto packet = writer.WriteInventoryResponse(&body);
        ASSERT_NE(packet, nullptr);
        ASSERT_EQ(packet->GetLength(), sizeof(PacketStruct<InventoryResBody>));
        const auto* response = reinterpret_cast<const PacketStruct<InventoryResBody>*>(packet->GetBuffer());
        EXPECT_EQ(response->body.resStatus, RES_STATUS::SUCCESS);
        EXPECT_EQ(response->body.itemCount, MAX_INVENTORY_ITEMS);
        for (uint16_t i = 0; i < body.itemCount; ++i) {
            EXPECT_EQ(response->body.items[i].itemID, body.items[i].itemID);
            EXPECT_EQ(response->body.items[i].quantity, body.items[i].quantity);
            EXPECT_EQ(response->body.items[i].slot, body.items[i].slot);
        }
    }
    TEST_F(NonZoneThreadPoolFixture, ZoneWireCarriesStampAndChunkCountWithoutLeakingFailedAck) {
        static_assert(sizeof(PacketStruct<ZoneChangeResponseBody>) == 47);
        static_assert(offsetof(FullSnapshotBody, states) == 29);
        static_assert(offsetof(DeltaSnapshotBody, updates) == 12);
        static_assert(offsetof(MonsterFullSnapshotBody, states) == 29);
        static_assert(offsetof(MonsterDeltaSnapshotBody, updates) == 12);
        static_assert(offsetof(ActionResultBody, actions) == 12);
        const uint64_t generation = 0x123456789ABCDEF0ULL;
        const std::shared_ptr<IPacket> packets[] = {
            writer.GetFullHeader(2, generation), writer.GetDeltaHeader(2, generation),
            writer.GetMonsterFullHeader(2, generation), writer.GetMonsterDeltaHeader(2, generation),
            writer.GetActionHeader(2, generation)
        };
        const uint8_t opcodes[] = {7, 8, 9, 10, 12};
        for (size_t i = 0; i < std::size(packets); ++i) {
            ASSERT_NE(packets[i], nullptr);
            const size_t baseLength = i == 0 || i == 2 ? 37 : 20;
            ASSERT_EQ(packets[i]->GetLength(), baseLength);
            auto* header = parseHeader(packets[i]->GetBuffer());
            EXPECT_EQ(header->opcode, opcodes[i]);
            EXPECT_EQ(header->flags, FLAG_SIMULATION);
            writer.AddChunk(packets[i], 86, 2);
            auto* body = parseBody<FullSnapshotBody>(packets[i]->GetBuffer());
            EXPECT_EQ(header->length, baseLength + 86);
            EXPECT_EQ(body->count, 2);
            EXPECT_EQ(body->zoneID, 2);
            EXPECT_EQ(body->zoneGeneration, generation);
        }
        auto ack = writer.WriteZoneChangeSucess(2, 99, 3, 100, 50, generation, 7);
        ASSERT_NE(ack, nullptr);
        ASSERT_EQ(ack->GetLength(), 47);
        EXPECT_EQ(parseBody<ZoneChangeResponseBody>(ack->GetBuffer())->zoneGeneration, generation);
        EXPECT_EQ(parseBody<ZoneChangeResponseBody>(ack->GetBuffer())->ownershipEpoch, 7);
        EXPECT_EQ(parseHeader(ack->GetBuffer())->flags, FLAG_SIMULATION);
        auto failed = writer.WriteZoneChangeFailed();
        ASSERT_NE(failed, nullptr);
        ASSERT_EQ(failed->GetLength(), 47);
        for (size_t i = sizeof(PacketHeader); i < failed->GetLength(); ++i)
            EXPECT_EQ(failed->GetBuffer()[i], 0);
        EXPECT_EQ(parseHeader(failed->GetBuffer())->flags, FLAG_SIMULATION);
    }

    TEST_F(NonZoneThreadPoolFixture, BootstrapPairsSameEpochAndSequenceIncludingEmptyMonsterFull) {
        PrepareZones();
        ASSERT_TRUE(LoadTransitionCharacter());
        ZoneChangeResult entry;
        ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, entry));
        ASSERT_TRUE(state->SendBootstrap(42, entry.ownershipEpoch));
        ASSERT_TRUE(state->SendBootstrap(42, entry.ownershipEpoch));
        EXPECT_EQ(BootstrapSequence(), 2);
        auto firstWork = PopBroadcastWork();
        auto secondWork = PopBroadcastWork();
        ASSERT_NE(firstWork, nullptr);
        ASSERT_NE(secondWork, nullptr);
        EXPECT_TRUE(firstWork->targeted);
        EXPECT_EQ(firstWork->targetSessionID, 42);
        EXPECT_EQ(firstWork->zoneID, 1);
        ASSERT_EQ(firstWork->headers.size(), 2);
        ASSERT_EQ(firstWork->chunks.size(), 2);
        const auto* playerHeader = parseBody<FullSnapshotBody>(firstWork->headers[0]->GetBuffer());
        const auto* monsterHeader = parseBody<MonsterFullSnapshotBody>(firstWork->headers[1]->GetBuffer());
        EXPECT_EQ(playerHeader->count, 1);
        EXPECT_EQ(monsterHeader->count, 0);
        EXPECT_EQ(playerHeader->zoneID, monsterHeader->zoneID);
        EXPECT_EQ(playerHeader->zoneGeneration, monsterHeader->zoneGeneration);
        EXPECT_EQ(playerHeader->ownershipEpoch, entry.ownershipEpoch);
        EXPECT_EQ(monsterHeader->ownershipEpoch, entry.ownershipEpoch);
        EXPECT_EQ(playerHeader->bootstrapID, 1);
        EXPECT_EQ(monsterHeader->bootstrapID, 1);
        EXPECT_EQ(playerHeader->bootstrapEnd, 1);
        EXPECT_EQ(monsterHeader->bootstrapEnd, 1);
        EXPECT_EQ(firstWork->chunks[1]->GetLength(), 0);
        EXPECT_EQ(playerHeader->states[0].zoneInternalID, entry.internalID);
        EXPECT_TRUE(secondWork->targeted);
        const auto* secondPlayer = parseBody<FullSnapshotBody>(secondWork->headers[0]->GetBuffer());
        const auto* secondMonster = parseBody<MonsterFullSnapshotBody>(secondWork->headers[1]->GetBuffer());
        EXPECT_EQ(playerHeader->bootstrapID, monsterHeader->bootstrapID);
        EXPECT_EQ(secondPlayer->bootstrapID, secondMonster->bootstrapID);
        EXPECT_LT(playerHeader->bootstrapID, secondPlayer->bootstrapID);
        EXPECT_EQ(playerHeader->ownershipEpoch, secondPlayer->ownershipEpoch);
    }

    TEST_F(NonZoneThreadPoolFixture, ClientDirectedZoneOpsAreRejectedAndEntryRelocationPreservesCharacterState) {
        PrepareZones();
        ASSERT_TRUE(LoadTransitionCharacter());
        ZoneChangeResult entry, rejected;
        ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, entry));
        EXPECT_EQ(entry.zoneID, 1);
        EXPECT_EQ(entry.generation, 1);
        EXPECT_EQ(entry.ownershipEpoch, 1);
        const auto original = CharacterIn(1);
        AddOldCast();
        for (uint8_t op : {ZONE_CHANGE::UP, ZONE_CHANGE::DOWN, ZONE_CHANGE::LEFT, ZONE_CHANGE::RIGHT})
            EXPECT_FALSE(state->ChangeZone(42, op, rejected));
        EXPECT_EQ(Characters(1), 1);
        EXPECT_EQ(Characters(2), 0);
        EXPECT_FALSE(OldCastRemoved());
        const auto returned = CharacterIn(1);
        EXPECT_EQ(returned.zoneInternalID, original.zoneInternalID);
        EXPECT_EQ(returned.hp, original.hp);
        EXPECT_EQ(returned.mp, original.mp);
        EXPECT_EQ(returned.x, original.x);
        EXPECT_EQ(returned.y, original.y);
        EXPECT_EQ(state->GetZoneID(42), 1);
        EXPECT_EQ(OwnershipEpoch(), entry.ownershipEpoch);
        // A different player's admission advances the Zone stamp without changing this session epoch.
        ASSERT_TRUE(LoadTransitionCharacter(43));
        ZoneChangeResult peer;
        ASSERT_TRUE(state->ChangeZone(43, ZONE_CHANGE::ENTER, peer));
        EXPECT_GT(peer.generation, entry.generation);
        EXPECT_EQ(Generation(1), 2);

        // Expansion invalidates this old Zone 5 coordinate; entry alone may choose a safe point.
        ASSERT_TRUE(LoadTransitionCharacter(44, 5, 131, 121, 0));
        const auto saved = CharacterInLobby(44);
        ZoneChangeResult relocated;
        ASSERT_TRUE(state->ChangeZone(44, ZONE_CHANGE::ENTER, relocated));
        EXPECT_EQ(relocated.zoneID, 5);
        EXPECT_FLOAT_EQ(relocated.x, ZONE_SIZE + 10.0f);
        EXPECT_FLOAT_EQ(relocated.y, ZONE_SIZE + 10.0f);
        const auto migrated = CharacterIn(5, 44);
        EXPECT_EQ(migrated.hp, 0); // Relocation is not recovery or resurrection.
        EXPECT_EQ(migrated.mp, 61);
        EXPECT_EQ(migrated.characterID, 5044u);
        EXPECT_EQ(migrated.sessionID, 44u);
        EXPECT_EQ(migrated.profileId, saved.profileId);
        EXPECT_EQ(migrated.profileVersion, saved.profileVersion);
        EXPECT_EQ(migrated.maxHp, saved.maxHp);
        EXPECT_EQ(migrated.maxMp, saved.maxMp);
        EXPECT_EQ(migrated.exp, saved.exp);
        EXPECT_EQ(migrated.level, saved.level);
        EXPECT_EQ(migrated.attack, saved.attack);
        EXPECT_EQ(migrated.dir, saved.dir);
        EXPECT_EQ(migrated.skillSlotCnt, 3);
        ASSERT_EQ(migrated.skillSlot.size(), 3u);
        EXPECT_EQ(migrated.skillSlot[0].skillID, 0);
        EXPECT_EQ(migrated.skillSlot[0].skillCoolDownTick, 17);
        EXPECT_EQ(migrated.skillSlot[1].skillID, 1);
        EXPECT_EQ(migrated.skillSlot[1].skillCoolDownTick, 21);
        EXPECT_EQ(migrated.skillSlot[2].skillID, 2);
        EXPECT_EQ(migrated.skillSlot[2].skillCoolDownTick, 0);
        EXPECT_TRUE(WorldMap::CanTraverse(migrated.x, migrated.y, migrated.x, migrated.y));
        EXPECT_FALSE(HasLobbyCharacter(44));

        // A blocked saved point inside a valid Zone uses the same entry-only policy.
        const float blockedX = WorldMap::OriginX + 0.5f * WorldMap::TileSize;
        const float blockedY = WorldMap::OriginY + 0.5f * WorldMap::TileSize;
        ASSERT_FALSE(WorldMap::CanTraverse(blockedX, blockedY, blockedX, blockedY));
        ASSERT_TRUE(LoadTransitionCharacter(45, 1, blockedX, blockedY));
        ASSERT_TRUE(state->ChangeZone(45, ZONE_CHANGE::ENTER, relocated));
        EXPECT_FLOAT_EQ(relocated.x, 10.0f);
        EXPECT_FLOAT_EQ(relocated.y, 10.0f);
    }

    TEST_F(NonZoneThreadPoolFixture, ServerMovementCrossesHysteresisAndStaleEpochCannotRepeatTransfer) {
        PrepareZones();
        ASSERT_TRUE(LoadTransitionCharacter());
        ZoneChangeResult entry, transfer, stale;
        ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, entry));
        SetMoveBudget(1, 10.0f);
        ActionRequestBody outward{0, 205.0f, 100.0f, 255, entry.ownershipEpoch};
        ASSERT_TRUE(state->ProcessAction(42, 1, outward, transfer));
        EXPECT_EQ(transfer.zoneID, 2);
        EXPECT_EQ(transfer.ownershipEpoch, entry.ownershipEpoch + 1);
        EXPECT_FLOAT_EQ(transfer.x, 205.0f); // Forward handoff occurs at core edge + 5.
        EXPECT_EQ(Characters(1), 0);
        EXPECT_EQ(Characters(2), 1);
        EXPECT_EQ(Generation(2), 1);
        EXPECT_EQ(OwnershipEpoch(), transfer.ownershipEpoch);

        ActionRequestBody reverse{3, 195.0f, 100.0f, 255, transfer.ownershipEpoch};
        SetMoveBudget(2, 10.0f);
        ASSERT_TRUE(state->ProcessAction(42, 2, reverse, stale));
        EXPECT_EQ(stale.zoneID, 1);
        EXPECT_EQ(stale.ownershipEpoch, transfer.ownershipEpoch + 1);
        EXPECT_EQ(state->GetZoneID(42), 1);
        EXPECT_FLOAT_EQ(stale.x, 195.0f); // Reverse handoff uses the new zone's core edge - 5.

        ActionRequestBody oldAction{3, 195.0f, 100.0f, 255, transfer.ownershipEpoch};
        ZoneChangeResult repair;
        ASSERT_TRUE(state->ProcessAction(42, 2, oldAction, repair));
        EXPECT_EQ(repair.zoneID, 1);
        EXPECT_EQ(repair.ownershipEpoch, stale.ownershipEpoch);
        EXPECT_EQ(repair.chatID, entry.chatID);
        EXPECT_EQ(state->GetZoneID(42), 1);
        EXPECT_EQ(OwnershipEpoch(), stale.ownershipEpoch);
    }

    TEST_F(NonZoneThreadPoolFixture, ExplicitRespawnRestoresVitalsAndMovesCrossZoneOrWithinTownWithoutChangingProgress) {
        PrepareZones();
        state->GetZone(TOWN_ZONE_ID)->InitializeMonster();
        state->GetZone(2)->InitializeMonster();
        VerifyTownAndHuntingSpawns();
        ASSERT_TRUE(LoadTransitionCharacter(42, 2, 210.0f, 100.0f, 0));
        ZoneChangeResult entry, respawn;
        ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, entry));
        UpdateCharacter(2, 42, [](CharacterState& character) {
            character.maxHp = 1200;
            character.maxMp = 340;
            character.mp = 17;
            character.exp = 98765;
            character.level = 12;
            character.attack = 77;
        });
        const RespawnToTownBody request{entry.ownershipEpoch};
        ASSERT_TRUE(state->RespawnToTown(42, request, respawn));
        EXPECT_EQ(respawn.zoneID, TOWN_ZONE_ID);
        EXPECT_EQ(respawn.ownershipEpoch, entry.ownershipEpoch + 1);
        EXPECT_FLOAT_EQ(respawn.x, TOWN_SPAWN_X);
        EXPECT_FLOAT_EQ(respawn.y, TOWN_SPAWN_Y);
        EXPECT_EQ(state->GetZoneID(42), TOWN_ZONE_ID);
        EXPECT_EQ(Characters(2), 0);
        EXPECT_EQ(Characters(TOWN_ZONE_ID), 1);
        const auto restored = CharacterIn(TOWN_ZONE_ID);
        EXPECT_EQ(restored.hp, 1200);
        EXPECT_EQ(restored.mp, 340);
        EXPECT_EQ(restored.exp, 98765u);
        EXPECT_EQ(restored.level, 12);
        EXPECT_EQ(restored.attack, 77);
        EXPECT_EQ(restored.characterID, 5042u);
        EXPECT_EQ(restored.lastZone, TOWN_ZONE_ID);
        EXPECT_EQ(restored.moveBudget, 0.0f);
        EXPECT_TRUE(WorldMap::CanTraverse(restored.x, restored.y, restored.x, restored.y));
        EXPECT_EQ(OwnershipEpoch(), respawn.ownershipEpoch);

        ASSERT_TRUE(LoadTransitionCharacter(43, TOWN_ZONE_ID, 60.0f, 60.0f, 0));
        ZoneChangeResult townEntry, sameZoneRespawn;
        ASSERT_TRUE(state->ChangeZone(43, ZONE_CHANGE::ENTER, townEntry));
        UpdateCharacter(TOWN_ZONE_ID, 43, [](CharacterState& character) {
            character.maxHp = 800;
            character.maxMp = 200;
            character.mp = 6;
        });
        ASSERT_TRUE(state->RespawnToTown(43, RespawnToTownBody{townEntry.ownershipEpoch}, sameZoneRespawn));
        EXPECT_EQ(sameZoneRespawn.ownershipEpoch, townEntry.ownershipEpoch + 1);
        EXPECT_EQ(CharacterIn(TOWN_ZONE_ID, 43).hp, 800);
        EXPECT_EQ(CharacterIn(TOWN_ZONE_ID, 43).mp, 200);
        EXPECT_FLOAT_EQ(CharacterIn(TOWN_ZONE_ID, 43).x, TOWN_SPAWN_X);
        EXPECT_FLOAT_EQ(CharacterIn(TOWN_ZONE_ID, 43).y, TOWN_SPAWN_Y);
    }

    TEST_F(NonZoneThreadPoolFixture, RespawnRejectsLivingAndStaleRequestsAndRepairsLostAckIdempotently) {
        PrepareZones();
        ASSERT_TRUE(LoadTransitionCharacter(44, 2, 210.0f, 100.0f, 100));
        ZoneChangeResult entry, respawn;
        ASSERT_TRUE(state->ChangeZone(44, ZONE_CHANGE::ENTER, entry));
        UpdateCharacter(2, 44, [](CharacterState& character) {
            character.maxHp = 1000;
            character.maxMp = 100;
        });
        EXPECT_FALSE(state->RespawnToTown(44, RespawnToTownBody{entry.ownershipEpoch}, respawn));
        EXPECT_EQ(state->GetZoneID(44), 2);
        UpdateCharacter(2, 44, [](CharacterState& character) { character.hp = 0; });
        EXPECT_FALSE(state->RespawnToTown(44, RespawnToTownBody{entry.ownershipEpoch - 1}, respawn));
        ASSERT_TRUE(state->RespawnToTown(44, RespawnToTownBody{entry.ownershipEpoch}, respawn));
        const auto committedEpoch = respawn.ownershipEpoch;
        const auto committedCharacter = CharacterIn(TOWN_ZONE_ID, 44);
        ZoneChangeResult retriedAck;
        ASSERT_TRUE(state->RespawnToTown(44, RespawnToTownBody{entry.ownershipEpoch}, retriedAck));
        EXPECT_EQ(retriedAck.ownershipEpoch, committedEpoch);
        const auto afterRetry = CharacterIn(TOWN_ZONE_ID, 44);
        EXPECT_EQ(afterRetry.hp, committedCharacter.hp);
        EXPECT_EQ(afterRetry.mp, committedCharacter.mp);
        EXPECT_FLOAT_EQ(afterRetry.x, committedCharacter.x);
        EXPECT_FLOAT_EQ(afterRetry.y, committedCharacter.y);
        EXPECT_EQ(OwnershipEpoch(44), committedEpoch);
        ActionRequestBody resume{0, afterRetry.x, afterRetry.y, 255, committedEpoch};
        ZoneChangeResult noTransition;
        ASSERT_TRUE(state->ProcessAction(44, TOWN_ZONE_ID, resume, noTransition));
        EXPECT_FALSE(state->RespawnToTown(44, RespawnToTownBody{entry.ownershipEpoch}, retriedAck));
        EXPECT_EQ(state->GetZoneID(44), TOWN_ZONE_ID);
        EXPECT_EQ(OwnershipEpoch(44), committedEpoch);
    }

    TEST_F(NonZoneThreadPoolFixture, AutomaticCornerHandoffUsesAuthoredCrossingAndOuterEdgeDoesNotWrap) {
        PrepareZones();
        ASSERT_NE(state->GetZone(5), nullptr);
        ASSERT_NE(state->GetZone(6), nullptr);
        EXPECT_EQ(state->GetZone(7), nullptr);
        ASSERT_TRUE(LoadTransitionCharacter(42, 1, ZONE_SIZE, ZONE_SIZE));
        ZoneChangeResult entry, diagonal;
        ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, entry));
        SetMoveBudget(1, 10.0f);
        ActionRequestBody corner{0, ZONE_SIZE + TRANSITION_BUFFER, ZONE_SIZE + TRANSITION_BUFFER, 255, entry.ownershipEpoch};
        ASSERT_TRUE(state->ProcessAction(42, 1, corner, diagonal));
        EXPECT_EQ(diagonal.zoneID, 5); // Authored direct corner crossing is enabled for 1→5.
        EXPECT_EQ(diagonal.ownershipEpoch, entry.ownershipEpoch + 1);
        EXPECT_EQ(state->GetZoneID(42), 5);

        ASSERT_TRUE(LoadTransitionCharacter(43, 3, 600.0f, 100.0f));
        ZoneChangeResult edgeEntry, edgeResult;
        ASSERT_TRUE(state->ChangeZone(43, ZONE_CHANGE::ENTER, edgeEntry));
        SetMoveBudget(3, 10.0f, 43);
        ActionRequestBody outward{0, 604.0f, 100.0f, 255, edgeEntry.ownershipEpoch};
        ASSERT_TRUE(state->ProcessAction(43, 3, outward, edgeResult));
        EXPECT_EQ(state->GetZoneID(43), 3); // The outermost column has no east owner; ID arithmetic must not wrap.
        EXPECT_EQ(Characters(3), 1);
        EXPECT_EQ(Characters(4), 0);
    }

    TEST_F(NonZoneThreadPoolFixture, FailedAutomaticTransferRestoresOriginalMovementAndSourceMembership) {
        for (int kind = 0; kind < 3; ++kind) {
            ResetZones();
            PrepareZones();
            ASSERT_TRUE(LoadTransitionCharacter());
            ZoneChangeResult result;
            ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, result));
            const uint64_t epoch = result.ownershipEpoch;
            SetMoveBudget(1, 10.0f);
            const auto original = CharacterIn(1);
            ExhaustDestination(2, kind);
        ActionRequestBody crossing{0, 205.0f, 100.0f, 255, epoch};
            EXPECT_FALSE(state->ProcessAction(42, 1, crossing, result));
            const auto retained = CharacterIn(1);
            EXPECT_EQ(state->GetZoneID(42), 1);
            EXPECT_EQ(retained.zoneInternalID, original.zoneInternalID);
            EXPECT_EQ(retained.hp, original.hp);
            EXPECT_EQ(retained.mp, original.mp);
            EXPECT_FLOAT_EQ(retained.x, original.x);
            EXPECT_FLOAT_EQ(retained.y, original.y);
            EXPECT_FLOAT_EQ(retained.moveBudget, original.moveBudget);
            EXPECT_EQ(retained.skillSlot[1].skillCoolDownTick, 21);
            EXPECT_EQ(Generation(1), 1);
            EXPECT_EQ(OwnershipEpoch(), epoch);
            state->Disconnect(42);
        }
    }

    TEST_F(NonZoneThreadPoolFixture, FailedInitialAdmissionPreservesLobbyAndClosedSessionCannotEnter) {
        PrepareZones();
        ASSERT_TRUE(LoadTransitionCharacter(43, 1, (std::numeric_limits<float>::quiet_NaN)(), 10));
        ZoneChangeResult invalidResult;
        EXPECT_FALSE(state->ChangeZone(43, ZONE_CHANGE::ENTER, invalidResult));
        EXPECT_TRUE(HasLobbyCharacter(43));
        EXPECT_TRUE(std::isnan(CharacterInLobby(43).x));
        EXPECT_EQ(state->GetZoneID(43), 0);
        ASSERT_TRUE(LoadTransitionCharacter(42, 2, 50, 50)); // Invalid saved coordinates can relocate.
        const auto saved = CharacterInLobby();
        ExhaustDestination(2, 0); // Capacity must still fail without consuming the lobby character.
        ZoneChangeResult result;
        EXPECT_FALSE(state->ChangeZone(42, ZONE_CHANGE::ENTER, result));
        EXPECT_TRUE(HasLobbyCharacter(42));
        EXPECT_EQ(state->GetZoneID(42), 0);
        EXPECT_EQ(Generation(2), 0);
        const auto retained = CharacterInLobby();
        EXPECT_FLOAT_EQ(retained.x, saved.x);
        EXPECT_FLOAT_EQ(retained.y, saved.y);
        EXPECT_EQ(retained.hp, saved.hp);
        EXPECT_EQ(retained.mp, saved.mp);
        EXPECT_EQ(retained.characterID, saved.characterID);
        EXPECT_EQ(retained.lastZone, saved.lastZone);
        EXPECT_EQ(retained.skillSlot[0].skillCoolDownTick, saved.skillSlot[0].skillCoolDownTick);
        state->Disconnect(42);
        EXPECT_FALSE(state->ChangeZone(42, ZONE_CHANGE::ENTER, result));
        EXPECT_FALSE(HasLobbyCharacter(42));
        EXPECT_EQ(Characters(2), MAX_ZONE_CAPACITY);
    }

    TEST_F(NonZoneThreadPoolFixture, TransferRacingDisconnectOrCleanupLeavesNoCharacterOrSession) {
        for (int cleanup = 0; cleanup < 2; ++cleanup) {
            ResetZones();
            PrepareZones();
            ASSERT_TRUE(LoadTransitionCharacter());
            ZoneChangeResult result;
            ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, result));
            SetMoveBudget(1, 10.0f);
            ActionRequestBody crossing{0, 205.0f, 100.0f, 255, result.ownershipEpoch};
            std::promise<void> start;
            auto gate = start.get_future().share();
            auto transfer = std::async(std::launch::async, [&, gate] {
                gate.wait();
                ZoneChangeResult ignored;
                state->ProcessAction(42, 1, crossing, ignored);
            });
            auto closing = std::async(std::launch::async, [&, gate, cleanup] {
                gate.wait();
                if (cleanup) RaceCleanup(); else state->Disconnect(42);
            });
            start.set_value();
            ASSERT_EQ(transfer.wait_for(2s), std::future_status::ready);
            ASSERT_EQ(closing.wait_for(2s), std::future_status::ready);
            transfer.get();
            closing.get();
            EXPECT_EQ(state->GetZoneID(42), -1);
            EXPECT_EQ(Characters(1), 0);
            EXPECT_EQ(Characters(2), 0);
            EXPECT_FALSE(state->ChangeZone(42, ZONE_CHANGE::ENTER, result));
        }
    }

    TEST_F(NonZoneThreadPoolFixture, CheatFlushDoesNotHoldZoneWhileWaitingForSessionShard) {
        PrepareZones();
        ASSERT_TRUE(LoadTransitionCharacter());
        ZoneChangeResult result;
        ASSERT_TRUE(state->ChangeZone(42, ZONE_CHANGE::ENTER, result));
        EXPECT_TRUE(FlushReleasesZoneBeforeWaitingForShard());
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarSearchValidatesFieldSessionFlagsLengthTypeAndRequestId) {
        SessionData ready{};
        ready.authenticated = ready.authCompleted = true;
        ready.userID = 6982;
        ready.characterID = 5011;
        ready.zoneID = 1;
        CountedView absent(42, BazaarSearchRequestBody{90, 1, 0});
        handler.Process(&absent);
        EXPECT_FALSE(loads.HasRequest());
        EXPECT_TRUE(iocp.sentOpcodes.empty());
        for (int invalid = 0; invalid < 11; ++invalid) {
            auto image = ready;
            BazaarSearchRequestBody request{91, 1, 65535};
            uint8_t flags = 0;
            int delta = 0;
            if (invalid == 0) image.authenticated = false;
            if (invalid == 1) image.authCompleted = false;
            if (invalid == 2) image.userID = 0;
            if (invalid == 3) image.characterID = 0;
            if (invalid == 4) image.zoneID = 0;
            if (invalid == 5) flags = FLAG_SIMULATION;
            if (invalid == 6) delta = -1;
            if (invalid == 7) delta = 1;
            if (invalid == 8) request.requestID = 0;
            if (invalid == 9) request.itemType = 0;
            if (invalid == 10) request.itemType = 4;
            SetSessionImage(image);
            CountedView view(42, request, flags, delta);
            handler.Process(&view);
            EXPECT_FALSE(loads.HasRequest()) << invalid;
            EXPECT_TRUE(iocp.sentOpcodes.empty()) << invalid;
        }
        SetSessionImage(ready);
        CountedView good(42, BazaarSearchRequestBody{92, 3, 65535});
        handler.Process(&good);
        auto request = loads.TakeRequest();
        ASSERT_NE(request, nullptr);
        EXPECT_EQ(request->GetLength(), sizeof(MsgStruct<MsgBazaarSearchBody>));
        EXPECT_EQ(parseMsgHeader(request->GetBuffer())->messageType, MSG_BAZAAR_SEARCH);
        const auto* body = parseMsgBody<MsgBazaarSearchBody>(request->GetBuffer());
        EXPECT_EQ(body->requestID, 92);
        EXPECT_EQ(body->item_type, 3);
        EXPECT_EQ(body->page, 65535);
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarSearchQueueRejectionReturnsCorrelatedFailureAndLateResponseDrops) {
        SessionData ready{};
        ready.authenticated = ready.authCompleted = true;
        ready.userID = 6982; ready.characterID = 5011; ready.zoneID = 1;
        SetSessionImage(ready);
        loads.accept = false;
        CountedView good(42, BazaarSearchRequestBody{123, 1, 0});
        handler.Process(&good);
        ASSERT_EQ(iocp.sentOpcodes.size(), 1);
        ASSERT_EQ(iocp.lastPacket.size(), 15);
        const auto* failed = parseBody<BazaarSearchResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(failed->requestID, 123);
        EXPECT_EQ(failed->resStatus, 0);
        EXPECT_EQ(failed->count, 0);
        FinishSearch(124);
        ASSERT_EQ(iocp.sentOpcodes.size(), 2);
        state->SetZoneID(42, 0); // This focused fixture has no Zone character to remove.
        state->Disconnect(42);
        FinishSearch(124);
        EXPECT_EQ(iocp.sentOpcodes.size(), 2);
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarSearchWriterPacksThirtyRowsAndDropsInvalidBodyWithoutTail) {
        MsgBazaarSearchResBody body{};
        body.requestID = 0x12345678;
        body.resStatus = 1;
        body.count = 30;
        for (uint16_t i = 0; i < body.count; ++i)
            body.listings[i] = { uint64_t{100} + i, uint64_t{200} + i, uint32_t{300} + i,
                static_cast<uint16_t>(i + 1), uint64_t{400} + i, uint64_t{500} + i, 0 };
        auto packet = writer.WriteBazaarSearchResponse(&body);
        ASSERT_NE(packet, nullptr);
        EXPECT_EQ(packet->GetLength(), 1185);
        auto* header = parseHeader(packet->GetBuffer());
        EXPECT_EQ(header->opcode, OP::BAZAAR_SEARCH_RES);
        EXPECT_EQ(header->length, 1185);
        EXPECT_EQ(header->flags, 0);
        EXPECT_EQ(packet->GetBuffer()[8], 0x78);
        auto* rows = reinterpret_cast<BazaarSearchListing*>(packet->GetBuffer() + 15);
        EXPECT_EQ(rows[29].listingID, 129);
        EXPECT_EQ(rows[29].sellerCharacterID, 229);
        EXPECT_EQ(rows[29].itemID, 329);
        EXPECT_EQ(rows[29].quantity, 30);
        EXPECT_EQ(rows[29].price, 429);
        EXPECT_EQ(rows[29].registeredAt, 529);
        for (int kind = 0; kind < 4; ++kind) {
            auto invalid = body;
            if (kind == 0) invalid.resStatus = 0;
            if (kind == 1) invalid.count = 31;
            if (kind == 2) invalid.resStatus = 2;
            if (kind == 3) invalid.listings[29].quantity = 0;
            auto failed = writer.WriteBazaarSearchResponse(&invalid);
            ASSERT_NE(failed, nullptr);
            EXPECT_EQ(failed->GetLength(), 15);
            auto* response = parseBody<BazaarSearchResponseBody>(failed->GetBuffer());
            EXPECT_EQ(response->requestID, body.requestID);
            EXPECT_EQ(response->count, 0);
            EXPECT_EQ(response->resStatus, 0);
        }
        body.count = 0;
        auto empty = writer.WriteBazaarSearchResponse(&body);
        ASSERT_NE(empty, nullptr);
        EXPECT_EQ(empty->GetLength(), 15);
        EXPECT_EQ(parseBody<BazaarSearchResponseBody>(empty->GetBuffer())->resStatus, 1);
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarPurchaseWireOwnershipAndQueueFailuresPreserveDeliveryMeaning) {
        SessionData ready{};
        ready.authenticated = ready.authCompleted = true;
        ready.userID = 6982; ready.characterID = 5011; ready.zoneID = 1;
        CountedView buy(42, BazaarBuyRequestBody{123, UINT64_MAX});
        CountedView outbox(42, BazaarCheckOutboxRequestBody{124});
        handler.Process(&buy); handler.Process(&outbox);
        EXPECT_FALSE(loads.HasRequest());
        SetSessionImage(ready);
        handler.Process(&buy);
        auto request = loads.TakeRequest(); ASSERT_NE(request, nullptr);
        const auto* purchased = parseMsgBody<MsgBazaarBuyBody>(request->GetBuffer());
        EXPECT_EQ(purchased->characterID, 5011); EXPECT_EQ(purchased->listingID, UINT64_MAX); EXPECT_EQ(purchased->requestID, 123);
        handler.Process(&outbox);
        request = loads.TakeRequest(); ASSERT_NE(request, nullptr);
        const auto* delivery = parseMsgBody<MsgBazaarCheckOutboxBody>(request->GetBuffer());
        EXPECT_EQ(delivery->characterID, 5011); EXPECT_EQ(delivery->requestID, 124);
        for (int invalid = 0; invalid < 7; ++invalid) {
            auto image = ready;
            if (invalid == 0) image.authenticated = false;
            if (invalid == 1) image.characterID = 0;
            SetSessionImage(image);
            const uint8_t flags = invalid == 2 ? FLAG_SIMULATION : 0;
            const int delta = invalid == 3 ? -1 : invalid == 4 ? 1 : 0;
            CountedView badBuy(42, BazaarBuyRequestBody{invalid == 5 ? 0U : 123U, invalid == 6 ? 0ULL : UINT64_MAX}, flags, delta);
            CountedView badOutbox(42, BazaarCheckOutboxRequestBody{invalid == 5 ? 0U : 124U}, flags, delta);
            if (invalid == 6) parseHeader(badOutbox.GetPtr())->length++;
            handler.Process(&badBuy); handler.Process(&badOutbox);
            EXPECT_FALSE(loads.HasRequest()) << invalid;
            EXPECT_TRUE(iocp.sentOpcodes.empty()) << invalid;
        }
        SetSessionImage(ready); loads.accept = false;
        handler.Process(&buy);
        ASSERT_EQ(iocp.lastPacket.size(), 37);
        const auto* unknown = parseBody<BazaarBuyResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(unknown->requestID, 123); EXPECT_EQ(unknown->listingID, UINT64_MAX);
        EXPECT_EQ(unknown->resStatus, 5); EXPECT_EQ(unknown->itemID, 0); EXPECT_EQ(unknown->quantity, 0); EXPECT_EQ(unknown->diamondSpent, 0);
        handler.Process(&outbox);
        ASSERT_EQ(iocp.lastPacket.size(), 25);
        EXPECT_EQ(parseBody<BazaarCheckOutboxResponseBody>(iocp.lastPacket.data())->requestID, 124);
        EXPECT_EQ(parseBody<BazaarCheckOutboxResponseBody>(iocp.lastPacket.data())->resStatus, 0);
        MsgBazaarBuyResBody bought{};
        bought.requestID = 123; bought.listingID = UINT64_MAX; bought.characterID = 5011;
        bought.resStatus = 1; bought.itemID = UINT32_MAX; bought.quantity = UINT32_MAX; bought.diamondSpent = UINT64_MAX;
        FinishBuy(bought);
        ASSERT_EQ(iocp.lastPacket.size(), 37);
        const auto* success = parseBody<BazaarBuyResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(success->quantity, UINT32_MAX); EXPECT_EQ(success->diamondSpent, UINT64_MAX);
        EXPECT_EQ(parseHeader(iocp.lastPacket.data())->opcode, OP::BAZAAR_BUY_RES);
        MsgBazaarCheckOutboxResBody partial{};
        partial.requestID = 124; partial.characterID = 5011; partial.deliveredCount = 2; partial.duplicatedCount = 1; partial.blockedCount = 1;
        FinishOutbox(partial);
        const auto* prefix = parseBody<BazaarCheckOutboxResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(prefix->resStatus, 0); EXPECT_EQ(prefix->deliveredCount, 2); EXPECT_EQ(prefix->duplicatedCount, 1); EXPECT_EQ(prefix->blockedCount, 1);
        EXPECT_EQ(parseHeader(iocp.lastPacket.data())->opcode, OP::BAZAAR_CHECK_OUTBOX_RES);
        ASSERT_EQ(iocp.sentOpcodes.size(), 4);
        bought.characterID = 5012; FinishBuy(bought);
        partial.characterID = 5012; FinishOutbox(partial);
        bought.characterID = 5011; partial.characterID = 5011;
        FinishBuy(bought, -1); FinishOutbox(partial, 1);
        state->SetZoneID(42, 0); state->Disconnect(42); FinishBuy(bought); FinishOutbox(partial);
        EXPECT_EQ(iocp.sentOpcodes.size(), 4);
        bought.resStatus = 2;
        auto invalid = writer.WriteBazaarBuyResponse(&bought); ASSERT_NE(invalid, nullptr);
        EXPECT_EQ(parseBody<BazaarBuyResponseBody>(invalid->GetBuffer())->resStatus, 5);
        EXPECT_EQ(parseBody<BazaarBuyResponseBody>(invalid->GetBuffer())->diamondSpent, 0);
        partial.deliveredCount = UINT32_MAX;
        EXPECT_EQ(writer.WriteBazaarCheckOutboxResponse(&partial), nullptr);

        CountedView cancel(42, BazaarCancelListingRequestBody{125, 987654321ULL});
        handler.Process(&cancel);
        EXPECT_FALSE(loads.HasRequest());
        loads.accept = true;
        SetSessionImage(ready);
        handler.Process(&cancel);
        request = loads.TakeRequest(); ASSERT_NE(request, nullptr);
        const auto* cancelRequest = parseMsgBody<MsgBazaarCancelListingBody>(request->GetBuffer());
        EXPECT_EQ(cancelRequest->requestID, 125); EXPECT_EQ(cancelRequest->listingID, 987654321ULL);
        EXPECT_EQ(cancelRequest->characterID, 5011);

        MsgBazaarCancelListingResBody cancelled{};
        cancelled.requestID = 125; cancelled.listingID = 987654321ULL; cancelled.characterID = 5011; cancelled.resStatus = 1;
        FinishCancelListing(cancelled);
        ASSERT_EQ(iocp.lastPacket.size(), 21);
        const auto* cancelWire = parseBody<BazaarCancelListingResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(cancelWire->requestID, 125); EXPECT_EQ(cancelWire->listingID, 987654321ULL);
        EXPECT_EQ(cancelWire->resStatus, 1); EXPECT_EQ(parseHeader(iocp.lastPacket.data())->opcode, OP::BAZAAR_CANCEL_LISTING_RES);
        EXPECT_EQ(iocp.sentOpcodes.size(), 5);
        cancelled.characterID = 5012; FinishCancelListing(cancelled);
        cancelled.characterID = 5011; FinishCancelListing(cancelled, -1);
        cancelled.resStatus = 2; EXPECT_EQ(writer.WriteBazaarCancelListingResponse(&cancelled), nullptr);
        cancelled.resStatus = 5;
        ASSERT_NE(writer.WriteBazaarCancelListingResponse(&cancelled), nullptr);

        struct CancelRows {
            int64_t result = 1, pending = 1;
            bool empty = false;
            bool next() { return !empty; }
            bool isNull(const char*) { return false; }
            int64_t getInt64(const char* column) { return std::string_view(column) == "result" ? result : pending; }
            uint64_t getUInt64(const char* column) {
                if (std::string_view(column) == "event_id") return 765;
                if (std::string_view(column) == "item_id") return 9;
                return 2;
            }
        } row;
        MsgBazaarCancelListingResBody parsed{};
        parsed.resStatus = 5;
        int returns = 0;
        Cache::ParseBazaarCancelResult(&row, [&](uint64_t eventID, uint32_t itemID, uint32_t quantity) {
            EXPECT_EQ(eventID, 765); EXPECT_EQ(itemID, 9); EXPECT_EQ(quantity, 2); ++returns;
            return false; // A blocked cache pass does not undo the committed listing cancellation.
        }, parsed);
        EXPECT_EQ(parsed.resStatus, 1); EXPECT_EQ(returns, 1);
        row.pending = 0; returns = 0;
        Cache::ParseBazaarCancelResult(&row, [&](uint64_t, uint32_t, uint32_t) { ++returns; }, parsed);
        EXPECT_EQ(parsed.resStatus, 1); EXPECT_EQ(returns, 0);
        row.result = 0;
        Cache::ParseBazaarCancelResult(&row, [&](uint64_t, uint32_t, uint32_t) { ++returns; }, parsed);
        EXPECT_EQ(parsed.resStatus, 0); EXPECT_EQ(returns, 0);
        row.empty = true;
        EXPECT_THROW(Cache::ParseBazaarCancelResult(&row, [](uint64_t, uint32_t, uint32_t) {}, parsed), std::runtime_error);
        EXPECT_EQ(parsed.resStatus, 5);
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarClaimWireOwnershipAndCorrelatedFailurePreserveFullWidthAmount) {
        SessionData ready{};
        ready.authenticated = ready.authCompleted = true;
        ready.userID = 6982; ready.characterID = 5011; ready.zoneID = 1;
        CountedView absent(42, BazaarClaimRequestBody{123, UINT64_MAX});
        handler.Process(&absent);
        EXPECT_FALSE(loads.HasRequest());
        SetSessionImage(ready);
        CountedView valid(42, BazaarClaimRequestBody{123, UINT64_MAX});
        handler.Process(&valid);
        auto request = loads.TakeRequest();
        ASSERT_NE(request, nullptr);
        const auto* claim = parseMsgBody<MsgBazaarClaimBody>(request->GetBuffer());
        EXPECT_EQ(claim->requestID, 123);
        EXPECT_EQ(claim->listingID, UINT64_MAX);
        EXPECT_EQ(claim->characterID, 5011);
        for (int invalid = 0; invalid < 12; ++invalid) {
            auto image = ready;
            BazaarClaimRequestBody body{123, UINT64_MAX};
            uint8_t flags = 0; int delta = 0;
            if (invalid == 0) image.authenticated = false;
            if (invalid == 1) image.authCompleted = false;
            if (invalid == 2) image.userID = 0;
            if (invalid == 3) image.characterID = 0;
            if (invalid == 4) image.zoneID = 0;
            if (invalid == 5) flags = FLAG_SIMULATION;
            if (invalid == 6) delta = -1;
            if (invalid == 7) delta = 1;
            if (invalid == 8) body.requestID = 0;
            if (invalid == 9) body.listingID = 0;
            SetSessionImage(image);
            CountedView view(42, body, flags, delta);
            if (invalid == 10) parseHeader(view.GetPtr())->length++;
            if (invalid == 11) parseHeader(view.GetPtr())->magic = 0;
            handler.Process(&view);
            EXPECT_FALSE(loads.HasRequest()) << invalid;
            EXPECT_TRUE(iocp.sentOpcodes.empty()) << invalid;
        }
        SetSessionImage(ready);
        loads.accept = false;
        handler.Process(&valid);
        ASSERT_EQ(iocp.lastPacket.size(), 29);
        const auto* failed = parseBody<BazaarClaimResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(failed->requestID, 123); EXPECT_EQ(failed->listingID, UINT64_MAX);
        EXPECT_EQ(failed->resStatus, 0); EXPECT_EQ(failed->diamondClaimed, 0);
        MsgBazaarClaimResBody response{};
        response.requestID = 124; response.listingID = UINT64_MAX;
        response.characterID = 5011; response.resStatus = 1; response.diamondClaimed = UINT64_MAX;
        FinishClaim(response);
        ASSERT_EQ(iocp.sentOpcodes.size(), 2);
        const auto* success = parseBody<BazaarClaimResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(success->diamondClaimed, UINT64_MAX);
        EXPECT_EQ(success->resStatus, 1);
        EXPECT_EQ(parseHeader(iocp.lastPacket.data())->opcode, OP::BAZAAR_CLAIM_RES);
        EXPECT_EQ(parseHeader(iocp.lastPacket.data())->flags, 0);
        response.characterID = 5012; FinishClaim(response);
        response.characterID = 5011; FinishClaim(response, -1);
        // This fixture has no field topology; use its initialized lobby for a real session removal.
        state->SetZoneID(42, 0);
        state->Disconnect(42); FinishClaim(response);
        EXPECT_EQ(iocp.sentOpcodes.size(), 2);
        response.resStatus = 4;
        auto duplicate = writer.WriteBazaarClaimResponse(&response);
        ASSERT_NE(duplicate, nullptr);
        EXPECT_EQ(parseBody<BazaarClaimResponseBody>(duplicate->GetBuffer())->diamondClaimed, 0);
        response.resStatus = 255;
        auto invalid = writer.WriteBazaarClaimResponse(&response);
        ASSERT_NE(invalid, nullptr);
        EXPECT_EQ(parseBody<BazaarClaimResponseBody>(invalid->GetBuffer())->resStatus, 0);
        response.requestID = 0;
        EXPECT_EQ(writer.WriteBazaarClaimResponse(&response), nullptr);
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarMyListUsesOnlyAuthenticatedFieldOwnerAndExactWireRequest) {
        SessionData ready{};
        ready.authenticated = ready.authCompleted = true;
        ready.userID = 6982; ready.characterID = 5011; ready.zoneID = 1;
        CountedView absent(42, BazaarMyListRequestBody{90});
        handler.Process(&absent);
        EXPECT_FALSE(loads.HasRequest());
        for (int invalid = 0; invalid < 10; ++invalid) {
            auto image = ready;
            BazaarMyListRequestBody request{91};
            uint8_t flags = 0;
            int delta = 0;
            if (invalid == 0) image.authenticated = false;
            if (invalid == 1) image.authCompleted = false;
            if (invalid == 2) image.userID = 0;
            if (invalid == 3) image.characterID = 0;
            if (invalid == 4) image.zoneID = 0;
            if (invalid == 5) flags = FLAG_SIMULATION;
            if (invalid == 6) delta = -1;
            if (invalid == 7) delta = 1;
            if (invalid == 8) request.requestID = 0;
            if (invalid == 9) image.zoneID = ZONE_COUNT + 1;
            SetSessionImage(image);
            CountedView view(42, request, flags, delta);
            handler.Process(&view);
            EXPECT_FALSE(loads.HasRequest()) << invalid;
            EXPECT_TRUE(iocp.sentOpcodes.empty()) << invalid;
        }
        SetSessionImage(ready);
        CountedView good(42, BazaarMyListRequestBody{92});
        handler.Process(&good);
        auto request = loads.TakeRequest();
        ASSERT_NE(request, nullptr);
        EXPECT_EQ(request->GetLength(), sizeof(MsgStruct<MsgBazaarMyListBody>));
        EXPECT_EQ(parseMsgHeader(request->GetBuffer())->messageType, MSG_BAZAAR_MY_LIST);
        const auto* body = parseMsgBody<MsgBazaarMyListBody>(request->GetBuffer());
        EXPECT_EQ(body->requestID, 92);
        EXPECT_EQ(body->characterID, 5011);
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarMyListQueueFailureAndStaleOwnerResponsesAreSafe) {
        SessionData ready{};
        ready.authenticated = ready.authCompleted = true;
        ready.userID = 6982; ready.characterID = 5011; ready.zoneID = 1;
        SetSessionImage(ready);
        loads.accept = false;
        CountedView good(42, BazaarMyListRequestBody{123});
        handler.Process(&good);
        ASSERT_EQ(iocp.sentOpcodes.size(), 1);
        ASSERT_EQ(iocp.lastPacket.size(), 15);
        const auto* failed = parseBody<BazaarMyListResponseBody>(iocp.lastPacket.data());
        EXPECT_EQ(failed->requestID, 123);
        EXPECT_EQ(failed->resStatus, 0);
        EXPECT_EQ(failed->count, 0);
        FinishMyList(124, 5011);
        ASSERT_EQ(iocp.sentOpcodes.size(), 2);
        FinishMyList(124, 5012);
        FinishMyList(124, 0);
        FinishMyList(124, 5011, -1);
        EXPECT_EQ(iocp.sentOpcodes.size(), 2);
        ready.characterID = 5012;
        SetSessionImage(ready);
        FinishMyList(124, 5011);
        EXPECT_EQ(iocp.sentOpcodes.size(), 2);
        FinishMyList(125, 5012);
        EXPECT_EQ(iocp.sentOpcodes.size(), 3);
        state->SetZoneID(42, 0);
        FinishMyList(125, 5012);
        state->Disconnect(42);
        FinishMyList(125, 5012);
        EXPECT_EQ(iocp.sentOpcodes.size(), 3);
    }

    TEST_F(NonZoneThreadPoolFixture, BazaarMyListWriterPacksStatesAndRejectsWrongOwnerOrInvalidRows) {
        MsgBazaarMyListResBody body{};
        body.requestID = 0x12345678;
        body.characterID = 5011;
        body.resStatus = 1;
        body.count = 30;
        for (uint16_t i = 0; i < body.count; ++i)
            body.listings[i] = { uint64_t{100} + i, 5011, uint32_t{300} + i,
                static_cast<uint16_t>(i + 1), uint64_t{400} + i, uint64_t{500} + i, static_cast<uint8_t>(i % 3) };
        auto packet = writer.WriteBazaarMyListResponse(&body);
        ASSERT_NE(packet, nullptr);
        EXPECT_EQ(packet->GetLength(), 1185);
        auto* header = parseHeader(packet->GetBuffer());
        EXPECT_EQ(header->opcode, OP::BAZAAR_MY_LIST_RES);
        EXPECT_EQ(header->length, 1185);
        EXPECT_EQ(header->flags, 0);
        EXPECT_EQ(packet->GetBuffer()[8], 0x78);
        auto* rows = reinterpret_cast<BazaarSearchListing*>(packet->GetBuffer() + 15);
        EXPECT_EQ(rows[29].listingID, 129);
        EXPECT_EQ(rows[29].sellerCharacterID, 5011);
        EXPECT_EQ(rows[29].itemID, 329);
        EXPECT_EQ(rows[29].quantity, 30);
        EXPECT_EQ(rows[29].price, 429);
        EXPECT_EQ(rows[29].registeredAt, 529);
        EXPECT_EQ(rows[29].status, 2);
        EXPECT_EQ(rows[28].status, 1);
        EXPECT_EQ(rows[27].status, 0);
        for (int kind = 0; kind < 7; ++kind) {
            auto invalid = body;
            if (kind == 0) invalid.resStatus = 0;
            if (kind == 1) invalid.count = 31;
            if (kind == 2) invalid.resStatus = 2;
            if (kind == 3) invalid.listings[29].quantity = 0;
            if (kind == 4) invalid.listings[29].status = 3;
            if (kind == 5) invalid.listings[29].sellerCharacterID = 5012;
            if (kind == 6) invalid.listings[29].listingID = 0;
            auto failed = writer.WriteBazaarMyListResponse(&invalid);
            ASSERT_NE(failed, nullptr);
            EXPECT_EQ(failed->GetLength(), 15);
            const auto* response = parseBody<BazaarMyListResponseBody>(failed->GetBuffer());
            EXPECT_EQ(response->requestID, body.requestID);
            EXPECT_EQ(response->count, 0);
            EXPECT_EQ(response->resStatus, 0);
        }
        body.count = 0;
        auto empty = writer.WriteBazaarMyListResponse(&body);
        ASSERT_NE(empty, nullptr);
        EXPECT_EQ(empty->GetLength(), 15);
        EXPECT_EQ(parseBody<BazaarMyListResponseBody>(empty->GetBuffer())->resStatus, 1);
        body.requestID = 0;
        EXPECT_EQ(writer.WriteBazaarMyListResponse(&body), nullptr);
    }

    TEST_F(NonZoneThreadPoolFixture, LateEnterCannotRebindCharacterInEmptyLobbyWindow) {
        ASSERT_TRUE(LoadTransitionCharacter());
        CharacterState original{};
        ASSERT_TRUE(lobby.EmigrateChar(42, original));
        EXPECT_EQ(state->GetZoneID(42), 0);
        auto other = original;
        ++other.characterID;
        EXPECT_FALSE(lobby.ImmigrateChar(42, other));
        EXPECT_TRUE(IsBoundTo(42, original.characterID));
        EXPECT_FALSE(HasLobbyCharacter(42));
        EXPECT_TRUE(lobby.ImmigrateChar(42, original));
        EXPECT_FALSE(lobby.ImmigrateChar(42, original)); // Existing same-character duplicate policy.
        EXPECT_TRUE(IsBoundTo(42, original.characterID));
    }

    TEST_F(NonZoneThreadPoolFixture, LateEnterResponseRacingInitialAdmissionCannotReplaceBoundCharacter) {
        PrepareZones();
        ASSERT_TRUE(LoadTransitionCharacter());
        Message request(sizeof(MsgStruct<MsgCharacterStateReqBody>));
        auto* st = reinterpret_cast<MsgStruct<MsgCharacterStateReqBody>*>(request.GetBuffer());
        *st = {};
        st->header.sessionID = 42;
        st->body.characterID = 9999;
        std::promise<void> start;
        auto gate = start.get_future().share();
        auto enter = std::async(std::launch::async, [&, gate] {
            gate.wait(); ZoneChangeResult result;
            return state->ChangeZone(42, ZONE_CHANGE::ENTER, result);
        });
        auto late = std::async(std::launch::async, [&, gate] {
            gate.wait(); return CompleteLoadSuccessfully(request);
        });
        start.set_value();
        ASSERT_EQ(enter.wait_for(2s), std::future_status::ready);
        ASSERT_EQ(late.wait_for(2s), std::future_status::ready);
        EXPECT_TRUE(enter.get());
        EXPECT_TRUE(late.get());
        EXPECT_TRUE(IsBoundTo(42, 5042));
        EXPECT_EQ(CharacterIn(1).characterID, 5042);
        EXPECT_FALSE(HasLobbyCharacter(42));
        const auto sent = iocp.sentOpcodes.size();
        st->body.characterID = 5042;
        ASSERT_TRUE(CompleteLoadSuccessfully(request));
        st->body.characterID = 9999;
        ASSERT_TRUE(CompleteLoadSuccessfully(request));
        EXPECT_EQ(iocp.sentOpcodes.size(), sent); // Same or different late field responses send nothing.
        EXPECT_TRUE(IsBoundTo(42, 5042));
    }

    TEST_F(NonZoneThreadPoolFixture, PendingEnterCompletionCannotRebindEmigratedCharacterOrInventory) {
        StartWorker();
        auto& authPacket = Enqueue(42);
        ASSERT_TRUE(auth.WaitFor(1));
        ASSERT_TRUE(authPacket.WaitForRelease());
        auth.Complete(0);

        // Both real worker requests reach the cache queue before either load completes.
        auto& firstEnter = EnqueueEnter(42, 5011);
        auto firstRequest = loads.TakeRequest();
        ASSERT_NE(firstRequest, nullptr);
        ASSERT_TRUE(firstEnter.WaitForRelease());
        auto& secondEnter = EnqueueEnter(42, 5012);
        auto secondRequest = loads.TakeRequest();
        ASSERT_NE(secondRequest, nullptr);
        ASSERT_TRUE(secondEnter.WaitForRelease());
        ASSERT_EQ(parseMsgBody<MsgCharacterStateReqBody>(firstRequest->GetBuffer())->characterID, 5011);
        ASSERT_EQ(parseMsgBody<MsgCharacterStateReqBody>(secondRequest->GetBuffer())->characterID, 5012);

        ASSERT_TRUE(CompleteLoadSuccessfully(*firstRequest));
        ASSERT_EQ(state->GetCharacterID(42), 5011);
        ASSERT_EQ(iocp.sentOpcodes.size(), 2);
        ASSERT_EQ(iocp.sentOpcodes.back(), OP::ENTER_WORLD_RESPONSE);
        CharacterState snapshot{};
        ASSERT_TRUE(lobby.EmigrateChar(42, snapshot));
        ASSERT_EQ(snapshot.characterID, 5011);
        ASSERT_EQ(state->GetZoneID(42), 0);
        ASSERT_FALSE(HasLobbyCharacter(42));

        // The late response uses the real MessageQueueHandler in the empty-lobby window.
        ASSERT_TRUE(CompleteLoadSuccessfully(*secondRequest));
        EXPECT_EQ(state->GetCharacterID(42), 5011);
        EXPECT_FALSE(HasLobbyCharacter(42));
        EXPECT_EQ(iocp.sentOpcodes.size(), 2); // No additional ENTER acknowledgement.

        views.push_back(std::make_unique<CountedView>(42, OP::INVENTORY_REQ));
        auto& inventoryPacket = *views.back();
        pool->EnqueueWork(std::unique_ptr<IPacketView, PacketViewDeleter>(&inventoryPacket));
        auto inventoryRequest = loads.TakeRequest();
        ASSERT_NE(inventoryRequest, nullptr);
        ASSERT_TRUE(inventoryPacket.WaitForRelease());
        ASSERT_EQ(parseMsgHeader(inventoryRequest->GetBuffer())->messageType, MSG_INVENTORY_REQ);
        EXPECT_EQ(parseMsgBody<MsgInventoryReqBody>(inventoryRequest->GetBuffer())->characterID, 5011);

        ASSERT_TRUE(lobby.ImmigrateChar(42, snapshot)); // Same-character rollback remains valid.
        EXPECT_FALSE(lobby.ImmigrateChar(42, snapshot)); // Existing duplicate policy remains valid.
        pool->EnqueueDisconnect(42);
        StopWorker();
        EXPECT_EQ(state->GetCharacterID(42), 0);
        EXPECT_FALSE(HasLobbyCharacter(42));
    }

}
