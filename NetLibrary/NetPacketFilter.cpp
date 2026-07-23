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
        if (op == Core::OP::PONG) {
            uint64_t rtt = packetDispatcher->GetRTT(std::move(pv), NetTimer::GetTimeMS());
            if (rtt > 200)
                perfCollector->AddJitterCnt();
            sessionManager->PongReceived(session, rtt);
            return true;
        }

        if (!(health & Core::MASK_EXIST)) {
            if (op == Core::OP::AUTH) {
                packetDispatcher->Process(std::move(pv));
                return true;
            }
            Core::gameLogger->LogWarn("net filter", "session not exist", "session", session);
            return false;
        }

        if (!(health & Core::MASK_NOT_CHEAT)) {
            Core::gameLogger->LogWarn("net filter", "cheat detect", "session", session);
            return false;
        }
        if (!(health & Core::MASK_AUTHENTICATED)) {
            Core::gameLogger->LogWarn("net filter", "session not authenticated", "session", session);
            switch (op) {
            case ::Core::OP::AUTH: {
                packetDispatcher->Process(std::move(pv));
                // 인증 실패를 바로 Disconnect 시키지 않기 때문에 재인증 허용
                return true;
            }
            case ::Core::OP::PONG: break;
            default: return false;
            }
        }

        switch (op)
        {
        default:
            packetDispatcher->Process(std::move(pv));
            break;
        }

        return true;
        
    }
}
