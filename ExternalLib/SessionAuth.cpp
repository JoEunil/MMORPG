#include "SessionAuth.h"
#include <iostream>
#include <thread>
#include "Config.h"
#include "Logger.h"

#include <CoreLib/LoggerGlobal.h>

namespace External {
    void SessionAuth::Initialize() {
        evthread_use_windows_threads(); // event_base를 multi-thread safe 모드로

        // 이벤트 루프 생성
        m_eventBase = event_base_new(); 
        
        // 비동기 컨텍스트 생성
        m_context = redisAsyncConnect(REDIS_AUTH_HOST, REDIS_AUTH_PORT);
        if (m_context->err) {
            Core::sysLogger->LogError(std::format("Redis connect failed: {}",  m_context->errstr));
        };

        m_context->data = this;

        // libevent와 연결
        redisLibeventAttach(m_context, m_eventBase);

        // 콜백 설정
        redisAsyncSetConnectCallback(m_context, [](const redisAsyncContext* c, int status) {
            // c 라이브러리라서 객체 포인터를 넘겨줄 수가 없음.
            auto self = reinterpret_cast<SessionAuth*>(c->data);
            if (status != REDIS_OK) {
                Core::sysLogger->LogError(std::string("Redis(session) connect error: ") + c->errstr);
                return ;
            }
            Core::sysLogger->LogError("Connected to Redis!(session)");
        });
        redisAsyncSetDisconnectCallback(m_context, [](const redisAsyncContext* c, int status) {
            auto self = reinterpret_cast<SessionAuth*>(c->data);
            Core::sysLogger->LogError(std::string("Redis(session) connect error: ") + std::to_string(status));
        });
        m_thread = std::thread([this]() {
            event_base_dispatch(m_eventBase);
        });
    }

    static void RedisCallback(redisAsyncContext* c, void* r, void* privdata) {
        uint8_t res = 0;

        auto* reply = static_cast<redisReply*>(r);
        auto* data = static_cast<Core::SessionCallbackData*>(privdata);

        if (reply && reply->type == REDIS_REPLY_STRING && std::memcmp(reply->str, data->sessionToken.data(), reply->len) == 0)
            res = 1;
        data->callbackFunc(data->sessionID, res, data->userID);
        delete data;
    }

    struct CommandEvent {
        redisAsyncContext* ctx;
        Core::SessionCallbackData* data;
    };

    void EventCallback(evutil_socket_t, short, void* arg) {
        auto* ev = static_cast<CommandEvent*>(arg);
        redisAsyncCommand(ev->ctx, RedisCallback, ev->data, "GET %llu", ev->data->userID);
        delete ev; // CommandEvent 해제
    }


    void SessionAuth::CheckSession(Core::SessionCallbackData* privdata) {
        auto* ev = new CommandEvent{ m_context, privdata };

        // EV_TIMEOUT | EV_PERSIST 안 붙이면 한 번만 실행됨
        struct event* libevent_ev = event_new(m_eventBase, -1, EV_TIMEOUT, EventCallback, ev);
        struct timeval tv { 0, 0 }; // 즉시 발생
        event_add(libevent_ev, &tv);
    }
}
