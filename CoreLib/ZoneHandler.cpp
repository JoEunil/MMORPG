#include "pch.h"
#include "ZoneHandler.h"

#include "IPacketView.h"
#include "ISessionAuth.h"
#include "IIOCP.h"
#include "IMessageQueue.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "Message.h"
#include "MessagePool.h"
#include "LoggerGlobal.h"

namespace Core {
    void ZoneHandler::ProcessAction(ActionRequestBody* body, uint64_t sessionID, uint16_t zoneID) {
        auto zoneState = stateManager->GetZone(zoneID);
        // 한 틱에서 마지막 입력으로 업데이트. 

        zoneState->Move(sessionID, body->dir, body->speed);
        zoneState->Skill(sessionID, body->skillSlot);
        zoneState->DirtyCheck(sessionID);
    }

    void ZoneHandler::Process(IPacketView* p, uint16_t zoneID) {
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode())
        {
            case OP::ACTION:
                ProcessAction(parseBody<ActionRequestBody>(p->GetPtr()), p->GetSessionID(), zoneID);
                break;
            default:
                errorLogger->LogInfo("zone Handler", "UnDefined OPCODE", "zone", zoneID, "opcode", p->GetOpcode(), "session", p->GetSessionID());
                break;
        }
    }
}
