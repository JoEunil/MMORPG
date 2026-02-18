#pragma once

#include <Windows.h>
#include <mutex>
#include <cstdint>

#include "TrafficFloodDetector.h"
#include "NetPacketFilter.h"
#include "Config.h"

#include <CoreLib/LoggerGlobal.h>
namespace Net {
    class SessionState {
        uint64_t m_sessionID = 0;
        // 네트워크(소켓) -> 게임 로직(세션)

        uint8_t m_sessionPingCount; // 누적 시 종료 (half-open)
        uint64_t m_rtt;

        bool m_flood = false;
        bool m_contextStatus = false;
        TrafficFloodDetector m_floodDetector;
        // recv가 한번씩 발생하고, recv 완료 후 다음 recv 호출이 이루어지기 때문에 atomic 플래그 하나로 충분
        // 경합 상황이면 flag 상태를 여러개로 나누고, CAS 함수가 필요했을 것.
        // 다음 recv에서 flood가 무조건 걸리도록 release, acquire만 잘 걸어주면 됨.

        bool NetStatus() {
            if (m_contextStatus == false) {
                Core::gameLogger->LogWarn("net session", "context invalid", "sessionID", m_sessionID);
                return false;
            }
            if (m_sessionPingCount > PING_COUNT_LIMIT) {
                Core::gameLogger->LogWarn("net session", "ping count exceed", "sessionID", m_sessionID, "pingCount", m_sessionPingCount);
                return false;
            }
            return true;
        }
        bool FloodCheck() {
            return m_flood;
        }
        mutable std::mutex m_mutex;
    public:
        uint64_t GetSessionID() const {
            return m_sessionID; 
        }
        void SetSession( uint64_t sessionID) {
            m_sessionID = sessionID;
            m_sessionPingCount = 0;
            m_rtt = 0;
            m_flood = false;
            m_contextStatus = true;
        }

        bool CheckSession() {
            if (!NetStatus())
                return false;
            if (FloodCheck()) {
                Core::gameLogger->LogWarn("net session", "Net Traffic Flood", "sessionID", m_sessionID);
                return false;
            }
            return true;
        }

        void PongReceived(uint64_t rtt) {
            m_sessionPingCount = 0;
            m_rtt = rtt;
        }
        
        uint64_t GetRtt() {
            // race condition 안중요함, 틀려도 되는 데이터
            return m_rtt;
        }

        void BufferReceived(uint32_t byte) {
            if (!m_floodDetector.ByteReceived(byte))
                m_flood = false;
        }

        void SetContextInvalid() {
            m_contextStatus = false;
        }
    };
}
