#include "pch.h"
#include "PacketDispatcher.h"
#include "PacketTypes.h"
#include "StateManager.h"
#include "LoggerGlobal.h"

namespace Core {
    void PacketDispatcher::Process(std::unique_ptr<IPacketView, PacketViewDeleter> pv) {
        PacketHeader* h = parseHeader(pv->GetPtr());
        
        uint64_t sessionID = pv->GetSessionID();
        
        
        bool isSimulation = (h->flags & FLAG_SIMULATION) == FLAG_SIMULATION;
        if (isSimulation) {
            int16_t zoneID = stateManager->GetZoneID(sessionID);
            if (zoneID <= 0) {
                errorLogger->LogWarn("packet dispatcher", "Invalid ZoneID", "zoneID", zoneID, "sessionID", pv->GetSessionID());
                return;
            }
            zoneThreadSet->EnqueueWork(std::move(pv), zoneID);
        } else {
            noneZoneThreadPool->EnqueueWork(std::move(pv));
        }
    }
    void PacketDispatcher::Disconnect(uint64_t sessionID) {
        noneZoneThreadPool->EnqueueDisconnect(sessionID);
    }
}
