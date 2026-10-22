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
    void ZoneHandler::ProcessAction(const ActionRequestBody* body, uint64_t sessionID, uint16_t zoneID) {
        ZoneChangeResult transition;
        if (!stateManager->ProcessAction(sessionID, zoneID, *body, transition) || transition.zoneID == 0)
            return;
        auto response = writer->WriteZoneChangeSucess(transition.zoneID, transition.chatID, transition.internalID,
            transition.x, transition.y, transition.generation, transition.ownershipEpoch);
        if (response)
            iocp->SendDataUnique(sessionID, std::move(response));
        stateManager->SendBootstrap(sessionID, transition.ownershipEpoch);
    }

    void ZoneHandler::Process(IPacketView* p, uint16_t zoneID) {
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode())
        {
            case OP::ACTION:
                if (p->GetLength() == sizeof(PacketStruct<ActionRequestBody>))
                    ProcessAction(parseBody<ActionRequestBody>(p->GetPtr()), p->GetSessionID(), zoneID);
                break;
            default:
                errorLogger->LogInfo("zone Handler", "UnDefined OPCODE", "zone", zoneID, "opcode", p->GetOpcode(), "session", p->GetSessionID());
                break;
        }
    }
}
