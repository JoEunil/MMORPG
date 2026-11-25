#pragma once

#include <cstring>
#include "../Interface/ICacheResponseWriter.h"

namespace Core {
    class MessageWriter : public ICacheResponseWriter {
    public:
        template<typename Body>
        bool Write(IMessage& message, MsgHeader header, const Body& body) const {
            return WriteResponse(message, header, body);
        }

    private:
        bool WriteEncoded(IMessage& message, std::span<const uint8_t> bytes) const override {
            if (message.GetCapacity() < bytes.size())
                return false;
            std::memcpy(message.GetBuffer(), bytes.data(), bytes.size());
            message.SetLength(static_cast<uint16_t>(bytes.size()));
            return true;
        }
    };
}
