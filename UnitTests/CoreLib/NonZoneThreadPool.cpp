#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <CoreLib/ISessionAuth.h>
#include <CoreLib/IIOCP.h>
#include <CoreLib/IPacketPool.h>
#include <CoreLib/LobbyZone.h>
#include <CoreLib/MessagePool.h>
#include <CoreLib/MessageQueueHandler.h>
#include <CoreLib/NonZoneThreadPool.h>
#include <CoreLib/PacketWriter.h>
#include <CoreLib/PacketTypes.h>
#include <CoreLib/StateManager.h>

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
            std::array<uint8_t, 1024> bytes;
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
            void SendData(uint64_t, std::shared_ptr<IPacket> packet) override {
                sentOpcodes.push_back(parseHeader(packet->GetBuffer())->opcode);
            }
            void SendDataChunks(uint64_t, std::shared_ptr<IPacket> packet,
                std::vector<std::shared_ptr<IPacket>>&) override {
                sentOpcodes.push_back(parseHeader(packet->GetBuffer())->opcode);
            }
            void SendDataUnique(uint64_t, std::unique_ptr<IPacket, PacketDeleter> packet) override {
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
            bool EnqueueMessage(Message* message) override {
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
        RecordingIOCP iocp;
        LobbyZone lobby;
        MessagePool messages;
        MessageQueueHandler responses;
        ControlledLoadQueue loads;
        std::vector<std::unique_ptr<CountedView>> views;

        void SetUp() override {
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
