#pragma once
#include <atomic>
#include <cstdint>
#include <thread>

#include <CoreLib/IPacketDispatcher.h>
#include <CoreLib/ICachedClock.h>
#include <CoreLib/LoggerGlobal.h>
#include "IAbortSocket.h"
namespace Net {
    class SessionManager;
    struct PingStruct {
        SOCKET socket;
        uint64_t session;
        uint64_t rtt;
        bool isAlive;
    };
    class PingManager {
        std::thread m_pingThread;
        std::atomic<bool> m_running = false;

        SessionManager* sessionManager = nullptr;
        IAbortSocket* abortSocket = nullptr;
        Core::IPacketDispatcher* packetDispatcher = nullptr;
        Core::ICachedClock* clock = nullptr;

        void Initialize(IAbortSocket* a, SessionManager* s, Core::IPacketDispatcher* p, Core::ICachedClock* c) {
            abortSocket = a;
            sessionManager = s;
            packetDispatcher = p;
            clock = c;
        }

        bool IsReady() const {
            if (abortSocket == nullptr) {
                Core::sysLogger->LogError("ping manager", "abortSocket not initialized");
                return false;
            }
            if (sessionManager == nullptr) {
                Core::sysLogger->LogError("ping manager", "sessionManager not initialized");
                return false;
            }
            if (packetDispatcher == nullptr) {
                Core::sysLogger->LogError("ping manager", "packetDispatcher not initialized");
                return false;
            }
            if (clock == nullptr) {
                Core::sysLogger->LogError("ping manager", "clock not initialized");
                return false;
            }
            return true;
        }
        void PingFunc();

        void PingStart() {
            m_running.store(true, std::memory_order_relaxed);
            m_pingThread = std::thread(&PingManager::PingFunc, this);
        }

        void StopPing() {
            m_running.store(false, std::memory_order_relaxed);
            if (m_pingThread.joinable())
                m_pingThread.join();
            Core::sysLogger->LogInfo("ping manager", "Ping thread stopped");
        }

        void CleanUp() {
            StopPing();
        }
        friend class Initializer;
    public:
    };
}
