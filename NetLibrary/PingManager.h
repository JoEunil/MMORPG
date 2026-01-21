#pragma once
#include <atomic>
#include <cstdint>
#include <unordered_map>

#include "IAbortSocket.h"
namespace Net {
    class SessionManager;
    class PingManager {
        std::thread m_pingThread;
        std::atomic<bool> m_running = false;

        SessionManager* sessionManager;
        IAbortSocket* abortSocket;

        void Initialize(IAbortSocket* a, SessionManager* s) {
            abortSocket = a;
            sessionManager = s;
        }

        void PingFunc();
        void HandleAbort(uint64_t session, bool result);
        void PingStart() {
            m_pingThread = std::thread(&PingManager::PingFunc, this);
        }

        void StopPing() {
            m_running.store(false);
            if (m_pingThread.joinable())
                m_pingThread.join();
        }

        void CleanUp() {
            StopPing();
        }
        friend class Initializer;

    public:
        void ReceivePong(uint64_t sessionID);
    };
}
