#pragma once

#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    class CacheResponseSender;
    class CacheStorageCurrency;

    class CurrencyHandler {
        CacheResponseSender* responses = nullptr;
        CacheStorageCurrency* cache_currency = nullptr;

        void Initialize(CacheResponseSender* sender, CacheStorageCurrency* dependency) {
            responses = sender;
            cache_currency = dependency;
        }
        bool IsReady() const {
            if (!responses || !cache_currency) {
                Core::sysLogger->LogError("currency handler", "dependencies not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    public:
        void Request(uint64_t sessionID, Core::MsgCurrencyReqBody request);
        void Deposit(uint64_t sessionID, Core::MsgCurrencyDepositBody request);
    };
}
