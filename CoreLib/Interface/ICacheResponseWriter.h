#pragma once

#include <span>
#include <type_traits>
#include "IMessage.h"
#include "../Cache/MessageTypes.h"

namespace Core {
    class ICacheResponseWriter {
    public:
        virtual ~ICacheResponseWriter() = default;

        template<typename Body>
        bool WriteResponse(IMessage& message, MsgHeader header, const Body& body) const {
            static_assert(sizeof(MsgStruct<Body>) <= MESSAGE_LEN);
            static_assert(std::is_trivially_copyable_v<MsgStruct<Body>>);
            // Preserve the complete existing message layout, including body alignment.
            const MsgStruct<Body> encoded{ header, body };
            return WriteEncoded(message, { reinterpret_cast<const uint8_t*>(&encoded), sizeof(encoded) });
        }

    private:
        virtual bool WriteEncoded(IMessage& message, std::span<const uint8_t> bytes) const = 0;
    };
}
