#pragma once

#include "../DB/DBConfig.h"
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    class CacheResponseSender;
    template<typename T> class DBWorker;

    class DiamondHandler {
        CacheResponseSender* responses = nullptr;
        DBWorker<DBConnectionBazaar>* dbWorkerBazaar = nullptr;

        void Initialize(CacheResponseSender* sender, DBWorker<DBConnectionBazaar>* dependency) {
            responses = sender;
            dbWorkerBazaar = dependency;
        }
        bool IsReady() const {
            if (!responses || !dbWorkerBazaar) {
                Core::sysLogger->LogError("diamond handler", "dependencies not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    public:
        void Request(uint64_t sessionID, Core::MsgDiamondReqBody request);
        void Deposit(uint64_t sessionID, Core::MsgDiamondDepositBody request);
    };
}
