#pragma once

#include "Config.h"
#include "DB/DBConfig.h"
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    class CacheResponseSender;
    class CacheStorageInventory;
    class BazaarRegistration;
    template<typename T> class DBWorker;

    class BazaarHandler {
        CacheResponseSender* responses = nullptr;
        DBWorker<DBConnectionBazaar>* dbWorkerBazaar = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;
        BazaarRegistration* bazaarRegistration = nullptr;

        template<typename Response, typename Work>
        void EnqueueQuery(Core::MsgHeader header, const Response& failureBody, const char* queueError, Work work);

        void Initialize(CacheResponseSender* sender, DBWorker<DBConnectionBazaar>* worker,
            CacheStorageInventory* inventory, BazaarRegistration* registration) {
            responses = sender;
            dbWorkerBazaar = worker;
            cache_inventory = inventory;
            bazaarRegistration = registration;
        }
        bool IsReady() const {
            if (!responses || !dbWorkerBazaar || !cache_inventory || !bazaarRegistration) {
                Core::sysLogger->LogError("bazaar handler", "dependencies not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    public:
        void MyList(uint64_t sessionID, Core::MsgBazaarMyListBody request);
        void Search(uint64_t sessionID, Core::MsgBazaarSearchBody request);
        void Register(uint64_t sessionID, Core::MsgBazaarRegisterBody request);
        void Cancel(uint64_t sessionID, Core::MsgBazaarCancelBody request);
        void Buy(uint64_t sessionID, Core::MsgBazaarBuyBody request);
        void CancelListing(uint64_t sessionID, Core::MsgBazaarCancelListingBody request);
        void Claim(uint64_t sessionID, Core::MsgBazaarClaimBody request);
        void OutboxList(uint64_t sessionID, Core::MsgBazaarOutboxListBody request);
        void CheckOutbox(uint64_t sessionID, Core::MsgBazaarCheckOutboxBody request);
    };
}
