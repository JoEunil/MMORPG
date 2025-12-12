#pragma once
#include <hiredis/include/hiredis.h>
#include <hiredis/include/adapters/libevent.h>

#include <event2/event.h>
#include <event2/thread.h>
#include <string>
#include <thread>
#include <CoreLib/ISessionAuth.h>

namespace External {
    class Logger;
    class SessionAuth : public Core::ISessionAuth {
        redisAsyncContext* m_context = nullptr;
        struct event_base* m_eventBase = nullptr;
        std::thread m_thread;
        Logger* logger;
    public:
        ~SessionAuth() {
            if (m_context) {
                redisAsyncDisconnect(m_context); 
            }
            if (m_eventBase) {
                event_base_loopexit(m_eventBase, nullptr); 
            }

            if (m_thread.joinable()) {
                m_thread.join();
            }
         }
        void Initialize(Logger* logger);
        void CheckSession(Core::SessionCallbackData* privdata) override;
    };
}
