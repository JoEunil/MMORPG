#include "pch.h"
#include "PacketDispatcher.h"
#include "PacketTypes.h"
#include "StateManager.h"
#include "ILogger.h"

namespace Core {
    void PacketDispatcher::Process(std::shared_ptr<IPacketView> pv) {
        PacketHeader* h = parseHeader(pv->GetPtr());
        
        uint64_t sessionID = pv->GetSessionID();
        
        
        bool isSimulation = (h->flags & FLAG_SIMULATION) == FLAG_SIMULATION;
        if (isSimulation) {
            int16_t zoneID = stateManager->GetZoneID(sessionID);
            if (zoneID <= 0) {
                logger->LogWarn(std::format("Invalid ZoneID, sessionID: {}", pv->GetSessionID()));
                return;
            }
            zoneThreadSet->EnqueueWork(pv, zoneID);
        } else {
            noneZoneThreadPool->EnqueueWork(pv);
        }
    }
    void PacketDispatcher::Disconnect(uint64_t sessionID) {
        noneZoneThreadPool->EnqueueDisconnect(sessionID);
    }
}
