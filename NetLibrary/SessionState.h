#pragma once

#include <Windows.h>
#include <cstdint>

#include "TrafficFloodDetector.h"
#include "NetPacketFilter.h"
#include "Config.h"

#include <CoreLib/LoggerGlobal.h>
namespace Net {
    class SessionState {
        uint64_t m_sessionID = 0;
        // 네트워크(소켓) -> 게임 로직(세션)

        uint8_t m_sessionPingCount = 0; // 누적 시 종료 (half-open)
        uint64_t m_rtt = 0;

        bool m_flood = false;
        bool m_contextStatus = false;
        TrafficFloodDetector m_floodDetector{};
        // SessionState는 자체 동기화를 두지 않는다.
        // 모든 접근이 SessionManager의 shard mutex 아래에서만 일어나므로
        // 멤버를 atomic으로 둘 필요가 없다. (SessionManager::CheckSession/PongReceived/
        // GetSessionSnapshot 등 진입점 전부가 shard mutex를 먼저 잡는다)

    public:
        uint64_t GetSessionID() const {
            return m_sessionID; 
        }

        void InitializeSession(uint64_t sessionID) {
            m_sessionID = sessionID;
            m_sessionPingCount = 0;
            m_rtt = 0;
            m_flood = false;
            m_contextStatus = true;
            m_floodDetector = TrafficFloodDetector{};
        }

        bool CheckSession() const {
            if (m_contextStatus == false) {
                Core::gameLogger->LogWarn("net session", "context invalid", "sessionID", m_sessionID);
                return false;
            }
            if (m_sessionPingCount >= PING_COUNT_LIMIT) {
                Core::gameLogger->LogWarn("net session", "ping count exceed", "sessionID", m_sessionID, "pingCount", m_sessionPingCount);
                return false;
            }
            if (m_flood) {
                Core::gameLogger->LogWarn("net session", "Net Traffic Flood", "sessionID", m_sessionID);
                return false;
            }
            return true;
        }

        void PongReceived(uint64_t rtt) {
            m_sessionPingCount = 0;
            m_rtt = rtt;
        }
        void PingSent() {
            if (m_sessionPingCount < PING_COUNT_LIMIT)
                ++m_sessionPingCount;
        }
        
        uint64_t GetRtt() const {
            // 직전 pong 기준이라 실제보다 낡을 수 있다. 표시·로깅용이라 무해.
            return m_rtt;
        }

        bool BufferReceived(uint32_t byte) {
            m_flood = m_flood || m_floodDetector.ByteReceived(byte);
            return !m_flood;
        }

        void SetContextInvalid() {
            m_contextStatus = false;
        }
    };
}
