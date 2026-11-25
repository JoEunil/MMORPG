#pragma once

namespace Net {
    class PacketView;

    class IBufferRelease {
    public:
        virtual ~IBufferRelease() = default;
        virtual void ReleaseBuffer(PacketView* packet) = 0;
    };
}
