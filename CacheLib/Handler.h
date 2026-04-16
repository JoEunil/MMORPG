#pragma once

#include <cstdint>

#include <mysqlconn/include/mysql/jdbc.h>
#include "CacheStorage5.h"
#include <CoreLib/LoggerGlobal.h>
namespace Core {
    class IMessageQueue;
    class ILogger;
    class Message;
}

namespace Cache {
    class MessagePool;
    class DBWorker;
    class Handler {
        Core::IMessageQueue* messageQ;  // response
        MessagePool* messagePool;
        CacheStorage5* cache_5;
        Core::ILogger* logger;
        DBWorker* dbWorker;
        void Initialize(Core::IMessageQueue* mq,  MessagePool* mp, DBWorker* d, CacheStorage5* c) {
            messageQ = mq;
            messagePool = mp;
            dbWorker = d;
            cache_5 = c;
        }
        bool IsReady() {
            if (messageQ == nullptr) {
                Core::sysLogger->LogError("cache handler", "messageQ not initialized");
                return false;
            }
            if (messagePool == nullptr) {
                Core::sysLogger->LogError("cache handler", "messagePool not initialized");
                return false;
            }
            if (dbWorker == nullptr) {
                Core::sysLogger->LogError("cache handler", "dbWorker not initialized");
                return false;
            }
            if (cache_5 == nullptr) {
                Core::sysLogger->LogError("cache handler", "cache_5 not initialized");
                return false;
            }
            return true;
        }
        void CharacterListRequest(Core::Message* msg, uint64_t sesionID, Core::MsgCharacterListReqBody* body);
        void CharacterStateRequest(Core::Message* msg, uint64_t sesionID, Core::MsgCharacterStateReqBody* body);
        void CharacterStateUpdate(Core::Message* msg, uint64_t sessionID, Core::MsgCharacterStateUpdateBody* body);
        void InventoryRequest(Core::Message* msg, uint64_t sesionID, Core::MsgInventoryReqBody* body);
        void InventoryUpdate(Core::Message* msg, uint64_t sesionID, Core::MsgInventoryUpdateBody* body);
        friend class Initializer;
    public:
        void Process(Core::Message* msg);
    };
}
