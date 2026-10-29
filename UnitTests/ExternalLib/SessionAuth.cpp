#include <winsock2.h>
#include <ws2tcpip.h>
#include <gtest/gtest.h>

#include <ExternalLib/SessionAuth.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    constexpr char token[] = "12345678-1234-1234-1234-123456789abc";

    struct Winsock {
        Winsock() {
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
                throw std::runtime_error("WSAStartup failed");
        }
        ~Winsock() { WSACleanup(); }
    };

    struct Socket {
        SOCKET value = INVALID_SOCKET;
        ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
        Socket() = default;
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;
    };

    // Each request is GET 1. Responses exercise the real hiredis parser on an isolated port.
    class RedisReplies {
        Winsock winsock;
        Socket listener;
        std::thread worker;
        uint16_t port = 0;
        std::promise<void> heldRequest;
        std::future<void> held = heldRequest.get_future();
    public:
        explicit RedisReplies(std::vector<std::optional<std::string>> responses) {
            listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener.value == INVALID_SOCKET)
                throw std::runtime_error("socket failed");
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
                || listen(listener.value, 1) != 0)
                throw std::runtime_error("loopback listener failed");
            int length = sizeof(address);
            if (getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &length) != 0)
                throw std::runtime_error("getsockname failed");
            port = ntohs(address.sin_port);
            worker = std::thread([this, responses = std::move(responses)] {
                fd_set readable;
                FD_ZERO(&readable);
                FD_SET(listener.value, &readable);
                timeval deadline{ 2, 0 };
                if (select(0, &readable, nullptr, nullptr, &deadline) <= 0)
                    return;
                Socket connection;
                connection.value = accept(listener.value, nullptr, nullptr);
                if (connection.value == INVALID_SOCKET)
                    return;
                DWORD timeout = 2000;
                setsockopt(connection.value, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout));
                setsockopt(connection.value, SOL_SOCKET, SO_SNDTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout));
                constexpr char command[] = "*2\r\n$3\r\nGET\r\n$1\r\n1\r\n";
                for (const auto& response : responses) {
                    std::array<char, sizeof(command) - 1> request{};
                    size_t received = 0;
                    while (received < request.size()) {
                        int count = recv(connection.value, request.data() + received,
                            static_cast<int>(request.size() - received), 0);
                        if (count <= 0)
                            return;
                        received += count;
                    }
                    if (std::memcmp(request.data(), command, request.size()) != 0 || !response)
                        return;
                    if (response->empty()) {
                        heldRequest.set_value();
                        char byte;
                        recv(connection.value, &byte, 1, 0); // Wait for Stop to close the pending command.
                        return;
                    }
                    size_t sent = 0;
                    while (sent < response->size()) {
                        int count = send(connection.value, response->data() + sent,
                            static_cast<int>(response->size() - sent), 0);
                        if (count <= 0)
                            return;
                        sent += count;
                    }
                }
            });
        }
        ~RedisReplies() { if (worker.joinable()) worker.join(); }
        uint16_t Port() const { return port; }
        bool WaitForHeldRequest() { return held.wait_for(2s) == std::future_status::ready; }
    };

    struct Completion {
        std::mutex mutex;
        std::condition_variable changed;
        std::vector<uint8_t> results;
        void Add(uint8_t result) {
            std::lock_guard<std::mutex> lock(mutex);
            results.push_back(result);
            changed.notify_all();
        }
        bool Wait() {
            std::unique_lock<std::mutex> lock(mutex);
            return changed.wait_for(lock, 2s, [this] { return !results.empty(); });
        }
        void ExpectOnce(uint8_t expected) {
            std::lock_guard<std::mutex> lock(mutex);
            ASSERT_EQ(results.size(), 1u);
            EXPECT_EQ(results.front(), expected);
        }
    };

    std::shared_ptr<Completion> Request(External::SessionAuth& auth, bool terminated = true) {
        auto completion = std::make_shared<Completion>();
        auto data = std::make_unique<Core::SessionCallbackData>();
        data->sessionID = 7;
        data->userID = 1;
        std::memcpy(data->sessionToken.data(), token, sizeof(token));
        if (!terminated)
            data->sessionToken.back() = 'x';
        data->callbackFunc = [completion](uint64_t, uint8_t result, uint64_t) {
            completion->Add(result);
        };
        auth.CheckSession(data.release());
        return completion;
    }

    std::string Bulk(const std::string& value) {
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    }
}

TEST(SessionAuthTest, OnlyExactTokenAndTerminatorAuthenticate) {
    RedisReplies server({ Bulk(token), Bulk(""), Bulk(std::string(token).substr(0, 12)),
        Bulk(std::string(token) + "extra"), Bulk(std::string(36, 'x')), "$-1\r\n", Bulk(token) });
    External::SessionAuth auth;
    auth.Initialize("127.0.0.1", server.Port());
    std::vector<std::shared_ptr<Completion>> completed;
    for (size_t i = 0; i < 7; ++i) {
        auto completion = Request(auth, i != 6);
        ASSERT_TRUE(completion->Wait()) << "response index " << i;
        completed.push_back(std::move(completion));
    }
    auth.Stop();
    for (size_t i = 0; i < completed.size(); ++i)
        completed[i]->ExpectOnce(i == 0 ? 1 : 0);
}

TEST(SessionAuthTest, DisconnectAndStopCompleteRequestsOnce) {
    {
        RedisReplies holdingServer({ std::string{} });
        External::SessionAuth pendingAuth;
        pendingAuth.Initialize("127.0.0.1", holdingServer.Port());
        auto pending = Request(pendingAuth);
        ASSERT_TRUE(holdingServer.WaitForHeldRequest());
        pendingAuth.Stop();
        ASSERT_TRUE(pending->Wait());
        pending->ExpectOnce(0);
    }
    RedisReplies server({ std::nullopt });
    External::SessionAuth auth;
    auto beforeInitialize = Request(auth);
    auth.Initialize("127.0.0.1", server.Port());
    auto disconnected = Request(auth);
    ASSERT_TRUE(disconnected->Wait());
    auto afterDisconnect = Request(auth);
    ASSERT_TRUE(afterDisconnect->Wait());
    auto atStop = Request(auth);
    const auto start = std::chrono::steady_clock::now();
    auth.Stop();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
    auto afterStop = Request(auth);
    auth.Stop();
    for (const auto& completion : { beforeInitialize, disconnected, afterDisconnect, atStop, afterStop }) {
        ASSERT_TRUE(completion->Wait());
        completion->ExpectOnce(0);
    }
}
