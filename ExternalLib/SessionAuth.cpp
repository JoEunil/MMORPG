#include "SessionAuth.h"
#include <cstring>
#include <stdexcept>
#include <utility>
#include "Config.h"

#include <CoreLib/LoggerGlobal.h>

namespace External {
    namespace {
        void Complete(std::unique_ptr<Core::SessionCallbackData> data, uint8_t result) {
            data->callbackFunc(data->sessionID, result, data->userID);
        }

        void RedisCallback(redisAsyncContext*, void* response, void* privdata) {
            std::unique_ptr<Core::SessionCallbackData> data(static_cast<Core::SessionCallbackData*>(privdata));
            const auto* reply = static_cast<redisReply*>(response);
            // Login/routes/auth.js stores a 36-byte UUID; the wire field adds a trailing zero.
            const size_t tokenLength = data->sessionToken.size() - 1;
            const bool matches = reply && reply->type == REDIS_REPLY_STRING && reply->str
                && reply->len == tokenLength && data->sessionToken.back() == 0
                && std::memcmp(reply->str, data->sessionToken.data(), tokenLength) == 0;
            Complete(std::move(data), matches ? 1 : 0);
        }
    }

    void SessionAuth::Initialize() {
        Initialize(REDIS_AUTH_HOST, REDIS_AUTH_PORT);
    }

    void SessionAuth::Initialize(const char* host, uint16_t port) {
        if (m_eventBase)
            throw std::logic_error("SessionAuth is already initialized");
        static std::once_flag threading;
        std::call_once(threading, [] {
            if (evthread_use_windows_threads() != 0)
                throw std::runtime_error("Failed to initialize libevent threading");
        });
        try {
            m_eventBase = event_base_new();
            if (!m_eventBase)
                throw std::runtime_error("Failed to create authentication event loop");
            m_commandEvent = event_new(m_eventBase, -1, EV_PERSIST,
                [](evutil_socket_t, short, void* self) {
                    static_cast<SessionAuth*>(self)->ProcessRequests();
                }, this);
            if (!m_commandEvent)
                throw std::runtime_error("Failed to create authentication command event");

            m_context = redisAsyncConnect(host, port);
            if (!m_context || m_context->err)
                throw std::runtime_error("Redis authentication connection failed");
            m_context->data = this;
            if (redisLibeventAttach(m_context, m_eventBase) != REDIS_OK)
                throw std::runtime_error("Failed to attach Redis authentication connection");
            if (redisAsyncSetConnectCallback(m_context, [](const redisAsyncContext* c, int status) {
                if (status != REDIS_OK) {
                    static_cast<SessionAuth*>(c->data)->m_context = nullptr;
                    if (Core::sysLogger)
                        Core::sysLogger->LogError("Redis(session) connect failed");
                }
            }) != REDIS_OK || redisAsyncSetDisconnectCallback(m_context, [](const redisAsyncContext* c, int) {
                static_cast<SessionAuth*>(c->data)->m_context = nullptr;
            }) != REDIS_OK)
                throw std::runtime_error("Failed to register Redis authentication callbacks");

            m_accepting = true;
            m_thread = std::thread([this] {
                // Keep servicing failed requests even after Redis removes its last event.
                event_base_loop(m_eventBase, EVLOOP_NO_EXIT_ON_EMPTY);
            });
        }
        catch (...) {
            Stop();
            throw;
        }
    }

    void SessionAuth::ProcessRequests() {
        std::vector<std::unique_ptr<Core::SessionCallbackData>> pending;
        bool stopping;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            pending.swap(m_pending);
            stopping = !m_accepting;
        }
        // The Redis context is accessed only on this loop, or after the thread has joined.
        for (auto& data : pending) {
            if (!stopping && m_context && redisAsyncCommand(m_context, RedisCallback, data.get(),
                "GET %llu", static_cast<unsigned long long>(data->userID)) == REDIS_OK) {
                data.release(); // hiredis now owns delivery of the completion callback.
            }
            else {
                Complete(std::move(data), 0);
            }
        }
        if (stopping) {
            if (auto* context = std::exchange(m_context, nullptr))
                redisAsyncFree(context); // Outstanding commands complete with a null reply.
            if (m_eventBase)
                event_base_loopbreak(m_eventBase);
        }
    }

    void SessionAuth::CheckSession(Core::SessionCallbackData* privdata) {
        std::unique_ptr<Core::SessionCallbackData> data(privdata);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_accepting) {
                m_pending.push_back(std::move(data));
                event_active(m_commandEvent, 0, 0);
                return;
            }
        }
        Complete(std::move(data), 0);
    }

    void SessionAuth::Stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_accepting = false;
            if (m_commandEvent)
                event_active(m_commandEvent, 0, 0);
        }
        if (m_thread.joinable())
            m_thread.join();
        ProcessRequests(); // Also covers initialization failure or an event-loop error.
        if (m_commandEvent) {
            event_free(m_commandEvent);
            m_commandEvent = nullptr;
        }
        if (m_eventBase) {
            event_base_free(m_eventBase);
            m_eventBase = nullptr;
        }
    }
}
