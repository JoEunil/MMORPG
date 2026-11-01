#pragma once
#include <cstdint>
#include <tuple>
#include <optional>

#include <CoreLib/PacketDispatcher.h>
#include "Jitter.h"

class ClientContextTest;

namespace Core {
    class IPacketView;
}
namespace Net {
    class SessionManager;
    class NetPerfCollector;
    class AccumulatedBuffer;
    enum class PacketValidation {
        Invalid, 
		Incomplete,
        Complete
    };
    class NetPacketFilter {
        static void Initialize(Core::IPacketDispatcher* p, SessionManager* s, NetPerfCollector* pc) {
            packetDispatcher = p;
            sessionManager = s;
            perfCollector = pc;
        };
        static bool IsReady() {
            if (packetDispatcher == nullptr) {
                Core::sysLogger->LogError("net filter", "packetDispatcher not initialized");
                return false;
            }
            if (sessionManager == nullptr) {
                Core::sysLogger->LogError("net filter", "sessionManager not initialized");
                return false;
            }
            if (perfCollector == nullptr) {
                Core::sysLogger->LogError("net filter", "perfCollector not initialized");
                return false;
            }
            return true;
        }

        static Core::PacketHeader ParseHeader(AccumulatedBuffer& accBuf);
        static bool CheckPacketValid(AccumulatedBuffer& accBuf, const Core::PacketHeader& header, uint64_t sessionID);

        inline static Core::IPacketDispatcher* packetDispatcher;
        inline static SessionManager* sessionManager;
        inline static NetPerfCollector* perfCollector;
        friend class Initializer;
        friend class ::ClientContextTest;

    public:
        static PacketValidation HasCompletePacket(AccumulatedBuffer& accBuf, Core::PacketHeader& header, uint64_t sessionId);
        static bool TryDispatch(std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter> pv, Jitter& jitter, std::optional<uint64_t>& pongRTT);
        // Context mutex를 해제한 뒤에만 SessionManager의 shard lock을 얻는다.
        static void NotifyPong(uint64_t sessionID, uint64_t rtt);
        static void Disconnect(uint64_t sessionID) {
            packetDispatcher->Disconnect(sessionID);
        }

        // called by PingManager
        static void Ping(uint64_t sessionID, uint64_t rtt, uint64_t nowMs) {
            packetDispatcher->Ping(sessionID, rtt, nowMs);
        }
    };

}
