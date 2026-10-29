#pragma once
#include <hiredis/include/hiredis.h>
#include <hiredis/include/adapters/libevent.h>

#include <event2/event.h>
#include <event2/thread.h>
#include <string>
#include <thread>
#include <memory>
#include <mutex>
#include <vector>
#include <CoreLib/ISessionAuth.h>

namespace External {
    class Logger;
    class SessionAuth : public Core::ISessionAuth {
        redisAsyncContext* m_context = nullptr;
        struct event_base* m_eventBase = nullptr;
        struct event* m_commandEvent = nullptr;
        std::thread m_thread;
        std::mutex m_mutex;
        std::vector<std::unique_ptr<Core::SessionCallbackData>> m_pending;
        bool m_accepting = false;
        void ProcessRequests();
    public:
        ~SessionAuth() { Stop(); }
        void Initialize();
        void Initialize(const char* host, uint16_t port);
        void Stop();
        void CheckSession(Core::SessionCallbackData* privdata) override;
    };
}
