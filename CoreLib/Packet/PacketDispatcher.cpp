#include "pch.h"
#include "PacketDispatcher.h"
#include "PacketTypes.h"
#include "../LoggerGlobal.h"

namespace Core {
    void PacketDispatcher::Process(std::unique_ptr<IPacketView, PacketViewDeleter> pv) {
        PacketHeader* h = parseHeader(pv->GetPtr());
        
        uint64_t sessionID = pv->GetSessionID();
        
        if (IsSimulationFlag(h->flags)) {
            int16_t zoneID = sessions->GetZoneID(sessionID);
            if (zoneID <= 0) {
                errorLogger->LogWarn("packet dispatcher", "Invalid ZoneID", {{"zoneID", zoneID}, {"sessionID", pv->GetSessionID()}});
                return;
            }
            zonePackets->TryEnqueuePacket(zoneID, std::move(pv));
        } else {
            nonSimulationQueue->EnqueuePacket(std::move(pv));
        }
    }
    void PacketDispatcher::Disconnect(uint64_t sessionID) {
        nonSimulationQueue->EnqueueDisconnect(sessionID);
    }
}
