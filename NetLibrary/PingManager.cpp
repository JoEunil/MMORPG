#include "pch.h"
#include "PingManager.h"
#include "SessionManager.h"
#include "ClientContext.h"
#include "NetTimer.h"
#include "Config.h"

#include <CoreLib//IPacket.h>

namespace Net {
    void PingManager::PingFunc() {
        auto tid = std::this_thread::get_id();
        Core::sysLogger->LogInfo( "ping manager", "Ping thread started", "threadID", tid);
        m_running.store(true);
        std::vector<PingStruct> clientList;
        // socket, session, rtt
        
        clientList.reserve(MAX_CLIENT_CONNECTION);
        while (m_running.load()) {
            // 여기서 NetSession 체크하고 Ping 송신
            for (int i = 0; i < SESSION_SHARD_SIZE; i++)
            {
                clientList.clear();
                sessionManager->GetSessionSnapshot(i, clientList);
                // 이 호출에서 ping count 증가시키기

                auto now = NetTimer::GetTimeMS();
                for (PingStruct& st: clientList)
                {
                    if (st.isAlive)
                        SendPing(st.session, st.rtt, now);
                    else
                        abortSocket->AbortSocket(st.socket);
                }
            }
            std::this_thread::sleep_for(PING_LOOP_WAIT);
        }
    }

    void PingManager::SendPing(uint64_t session, uint64_t rtt, uint64_t nowMs) {
        NetPacketFilter::Ping(session, rtt, nowMs);
    }
}