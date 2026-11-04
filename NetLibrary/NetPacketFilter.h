#pragma once
#include <cstdint>
#include <tuple>
#include <optional>

#include <CoreLib/PacketDispatcher.h>
#include <CoreLib/ICachedClock.h>
#include "Jitter.h"

class ClientContextTest;

namespace Core {
    class IPacketView;
}
namespace Net {
    class NetPerfCollector;
    class AccumulatedBuffer;
    enum class PacketValidation {
        Invalid, 
		Incomplete,
        Complete
    };
    class NetPacketFilter {
        static void Initialize(Core::IPacketDispatcher* p, NetPerfCollector* pc, Core::ICachedClock* c) {
            packetDispatcher = p;
            perfCollector = pc;
            clock = c;
        };
        static bool IsReady() {
            if (packetDispatcher == nullptr) {
                Core::sysLogger->LogError("net filter", "packetDispatcher not initialized");
                return false;
            }
            if (perfCollector == nullptr) {
                Core::sysLogger->LogError("net filter", "perfCollector not initialized");
                return false;
            }
            if (clock == nullptr) {
                Core::sysLogger->LogError("net filter", "clock not initialized");
                return false;
            }
            return true;
        }

        static Core::PacketHeader ParseHeader(AccumulatedBuffer& accBuf);
        static bool CheckPacketValid(AccumulatedBuffer& accBuf, const Core::PacketHeader& header, uint64_t sessionID);

        inline static Core::IPacketDispatcher* packetDispatcher;
        inline static NetPerfCollector* perfCollector;
        inline static Core::ICachedClock* clock = nullptr;
        friend class Initializer;
        friend class ::ClientContextTest;

    public:
        static PacketValidation HasCompletePacket(AccumulatedBuffer& accBuf, Core::PacketHeader& header, uint64_t sessionId);
        static bool TryDispatch(std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter> pv, Jitter& jitter, std::optional<uint64_t>& pongRTT);
        static void Disconnect(uint64_t sessionID) {
            packetDispatcher->Disconnect(sessionID);
        }

    };

}
