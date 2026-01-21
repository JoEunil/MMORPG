#include "pch.h"
#include "PingManager.h"
#include "SessionManager.h"
#include "ClientContext.h"

#include <CoreLib//IPacket.h>

namespace Net {
    void PingManager::PingFunc() {
        m_running.store(true);
        while (m_running.load()) {
            // 여기서 NetSession 체크하고 Ping 송신
            sessionManager->ForEachShard([this](uint64_t s, bool r) {
                this->HandleAbort(s, r);
                });

            std::this_thread::sleep_for(std::chrono::seconds(PING_LOOP_WAIT));
        }
    }

    void PingManager::HandleAbort(uint64_t session, bool result) {
        if (!result) {
            std::cout << "Ping check faild \n";
            abortSocket->AbortSocket(session);
        }
    }

    void PingManager::ReceivePong(uint64_t sessionID) {
        sessionManager->Pong(sessionID);
    }
}