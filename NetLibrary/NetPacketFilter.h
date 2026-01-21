#pragma once
#include <cstdint>
#include <tuple>

#include <CoreLib/PacketDispatcher.h>

namespace Core {
    class IPacketView;
}
namespace Net {
    class ClientContext;
    class NetPacketFilter {
        static void Initialize(Core::IPacketDispatcher* p) {
            packetDispatcher = p;
        };
        static bool IsReady() {
            return packetDispatcher != nullptr;
        }
        inline static Core::IPacketDispatcher* packetDispatcher;
        friend class Initializer;

    public:
        static bool TryDispatch(std::shared_ptr<Core::IPacketView>, ClientContext& ctx);
        static void Disconnect(uint64_t sessionID) {
            packetDispatcher->Disconnect(sessionID);
        }
        static void Ping(uint64_t sessionID, uint64_t rtt, std::chrono::steady_clock::time_point now) {
            packetDispatcher->Ping(sessionID, rtt, now);
        }
    };

}