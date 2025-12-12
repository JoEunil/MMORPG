#include "pch.h"
#include "ZoneHandler.h"

#include "IPacketView.h"
#include "ILogger.h"
#include "ISessionAuth.h"
#include "IIOCP.h"
#include "IMessageQueue.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "Message.h"
#include "MessagePool.h"

namespace Core {
    void ZoneHandler::ProcessAction(ActionRequestBody* body, uint64_t sessionID, uint16_t zoneID) {
        auto zoneState = stateManager->GetZone(zoneID);
        zoneState->Move(sessionID, body->dir, body->speed);
    }

    void ZoneHandler::Process(IPacketView* p, uint16_t zoneID) {
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode())
        {
            case OP::ACTION:
                ProcessAction(parseBody<ActionRequestBody>(p->GetPtr()), p->GetSessionID(), zoneID);
                break;
            default:
                logger->LogInfo(std::format("zone handler UnDefined OPCODE {}, {}, session {}, flag {}, magic {}", p->GetOpcode(), h->opcode,  p->GetSessionID(), h->flags, h->magic));
                break;
        }
    }
}
