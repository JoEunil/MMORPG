#pragma once
#include <cstdint>
#include <tuple>

#include <CoreLib/PacketDispatcher.h>

namespace Core {
    class IPacketView;
}
namespace Net {
    class SessionManager;
    class NetPacketFilter {
        static void Initialize(Core::IPacketDispatcher* p, SessionManager* s) {
            packetDispatcher = p;
            sessionManager = s;
        };
        static bool IsReady() {
            return packetDispatcher != nullptr;
        }
        inline static Core::IPacketDispatcher* packetDispatcher;
        inline static SessionManager* sessionManager;
        friend class Initializer;

    public:
        static bool TryDispatch(std::shared_ptr<Core::IPacketView> p);
        static void Disconnect(uint64_t sessionID) {
            packetDispatcher->Disconnect(sessionID);
        }

        // called by PingManager
        static void Ping(uint64_t sessionID, uint64_t rtt, uint64_t nowMs) {
            packetDispatcher->Ping(sessionID, rtt, nowMs);
        }
    };

}