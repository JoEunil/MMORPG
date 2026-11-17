#include "pch.h"
#include "PacketDispatcher.h"
#include "PacketTypes.h"
#include "SessionManager.h"
#include "LoggerGlobal.h"
#include "ZoneThread.h"

namespace Core {
    void PacketDispatcher::Process(std::unique_ptr<IPacketView, PacketViewDeleter> pv) {
        PacketHeader* h = parseHeader(pv->GetPtr());
        
        uint64_t sessionID = pv->GetSessionID();
        
        if (IsSimulationFlag(h->flags)) {
            int16_t zoneID = sessionManager->GetZoneID(sessionID);
            if (zoneID <= 0) {
                errorLogger->LogWarn("packet dispatcher", "Invalid ZoneID", "zoneID", zoneID, "sessionID", pv->GetSessionID());
                return;
            }
            ZoneEvent event{ZoneEventType::Packet, std::move(pv)};
            zoneEvents->TryEnqueue(zoneID, event);
        } else {
            nonZoneThreadPool->EnqueueWork(std::move(pv));
        }
    }
    void PacketDispatcher::Disconnect(uint64_t sessionID) {
        nonZoneThreadPool->EnqueueDisconnect(sessionID);
    }
}
