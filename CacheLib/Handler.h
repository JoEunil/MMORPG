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
    class DBConnectionPool;
    class MessagePool;
    class Handler {
        Core::IMessageQueue* messageQ;  // response
        MessagePool* messagePool;
        DBConnectionPool* connectionPool;
        CacheStorage5* cache_5;
        Core::ILogger* logger;
        void Initialize(Core::IMessageQueue* mq,  MessagePool* mp, DBConnectionPool* conn, CacheStorage5* c) {
            messageQ = mq;
            messagePool = mp;
            connectionPool = conn;
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
            if (connectionPool == nullptr) {
                Core::sysLogger->LogError("cache handler", "connectionPool not initialized");
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
