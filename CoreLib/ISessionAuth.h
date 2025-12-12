#pragma once

#include <string>
#include <thread>
#include <functional>
#include <array>

namespace Core {
    struct SessionCallbackData {
        std::string key;
        uint64_t userID;
        std::array<uint8_t, 37> sessionToken;
        uint64_t sessionID;
        std::function<void(uint64_t, uint8_t, uint64_t)> callbackFunc;
    };

    class ISessionAuth
    {
    public:
        virtual ~ISessionAuth() = default;
        virtual void CheckSession(SessionCallbackData* privdata) = 0;
    };
}
