#include "pch.h"
#include "NetPacketFilter.h"
#include "SessionManager.h"
#include "NetTimer.h"

#include <CoreLib/IPacketView.h>
#include <CoreLib/IPacketDispatcher.h>
#include <CoreLib/Config.h>
#include "NetPerfCollector.h"

namespace Net {
    bool NetPacketFilter::TryDispatch(std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter> pv) {        
        // 3차 패킷 검증
        auto session = pv->GetSessionID();
        auto op = pv->GetOpcode();
        // 게임 세션 상태 확인. 
        uint8_t health = packetDispatcher->HealthCheck(session);

        if (!(health & Core::MASK_EXIST)) {
            Core::gameLogger->LogWarn("net filter", "session not exist", "session", session);
            packetDispatcher->Process(std::move(pv));
            return true;
        }
        if (!(health & Core::MASK_NOT_CHEAT)) {
            Core::gameLogger->LogWarn("net filter", "cheat detect", "session", session);
            std::cout << "cheat detect";
            return false;
        }
        if (!(health & Core::MASK_AUTHENTICATED)) {
            Core::gameLogger->LogWarn("net filter", "session not authenticated", "session", session);
            switch (op) {
            case ::Core::OP::AUTH: 
                return true;
            // 인증 패킷이 중복으로 들어온것을 바로 Cheat로 판단하기는 어려움.
            case ::Core::OP::PONG: break;
            default: return false;
            }
        }

        switch (op)
        {
        case Core::OP::PONG: 
            {
                uint64_t rtt = packetDispatcher->GetRTT(std::move(pv), NetTimer::GetTimeMS());
                if (rtt > 200)
                    perfCollector->AddJitterCnt();
                sessionManager->PongReceived(session, rtt);
            }
            break;
        default:
            packetDispatcher->Process(std::move(pv));
            break;
        }

        return true;
        
    }
}
