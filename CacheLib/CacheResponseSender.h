#pragma once

#include "Config.h"
#include <CoreLib/Interface/ICacheResponseWriter.h>
#include <CoreLib/Interface/IMessagePool.h>
#include <CoreLib/Interface/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    class CacheResponseSender {
        Core::IMessageQueue* messageQ = nullptr;
        const Core::ICacheResponseWriter* writer = nullptr;
        Core::IMessagePool* messagePool = nullptr;

        void Initialize(Core::IMessageQueue* queue, const Core::ICacheResponseWriter* responseWriter,
            Core::IMessagePool* pool) {
            messageQ = queue;
            writer = responseWriter;
            messagePool = pool;
        }

        bool IsReady() const {
            if (messageQ == nullptr) {
                Core::sysLogger->LogError("cache response", "message queue not initialized");
                return false;
            }
            if (writer == nullptr) {
                Core::sysLogger->LogError("cache response", "response writer not initialized");
                return false;
            }
            if (messagePool == nullptr) {
                Core::sysLogger->LogError("cache response", "message pool not initialized");
                return false;
            }
            return true;
        }

        friend class Initializer;
    public:
        enum class Result { Queued, PoolExhausted, Rejected };

        template<typename Body>
        Result Send(Core::MsgHeader header, const Body& body) const {
            auto message = messagePool->AcquireScoped();
            if (!message)
                return Result::PoolExhausted;
            static_assert(sizeof(Core::MsgStruct<Body>) <= MESSAGE_LEN);
            if (writer->WriteResponse(*message, header, body)
                && messageQ->EnqueueMessage(message.get()))
                return Result::Queued;
            return Result::Rejected;
        }
    };
}
