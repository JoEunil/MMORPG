#include "pch.h"
#include "NetPacketFilter.h"
#include "ClientContext.h"
#include "NetTimer.h"

#include <CoreLib/IPacketView.h>
#include <CoreLib/IPacketDispatcher.h>
#include <CoreLib/Config.h>

namespace Net {
    bool NetPacketFilter::TryDispatch(std::shared_ptr<Core::IPacketView> pv, ClientContext& ctx) {        
        // 3차 패킷 검증
        auto session = pv->GetSessionID();
        auto op = pv->GetOpcode();
        // 게임 세션 상태 확인. 
        uint8_t health = packetDispatcher->HealthCheck(session);

        if (!(health & Core::MASK_EXIST)) {
            std::cout << "session not exist";
            packetDispatcher->Process(pv);
            return true;
        }
        if (!(health & Core::MASK_NOT_CHEAT)) {
            std::cout << "cheat detect";
            return false;
        }
        if (!(health & Core::MASK_AUTHENTICATED)) {
            std::cout << "session not authenticated";
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
                std::cout << "Pong\n";
                uint64_t rtt = packetDispatcher->GetRTT(pv, NetTimer::GetTimeMS());
                ctx.PongReceived();
                ctx.SetRtt(rtt);
            }
            break;
        default:
            std::cout << "dispatch " << (int)op << '\n';
            packetDispatcher->Process(pv);
            break;
        }

        return true;
        
    }
}
