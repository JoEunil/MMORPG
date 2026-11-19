#pragma once

#include "Message.h"
#include "MessageTypes.h"

namespace Core {
    class MessageWriter {
    public:
        template<typename Body>
        bool Write(Message& message, MsgHeader header, const Body& body) const {
            static_assert(sizeof(MsgStruct<Body>) <= MESSAGE_LEN);
            constexpr auto length = static_cast<uint16_t>(sizeof(MsgStruct<Body>));
            if (message.GetCapacity() < length)
                return false;

            auto* typedMessage = reinterpret_cast<MsgStruct<Body>*>(message.GetBuffer());
            typedMessage->header = header;
            typedMessage->body = body;
            message.SetLength(length);
            return true;
        }
    };
}
