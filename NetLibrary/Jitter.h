#pragma once
#include <cstdint>

namespace Net {
    class Jitter {
        // Jacobson/Karels 알고리즘, jitter 계산을 위함.
        uint64_t m_srtt = 0; // EWMA 평균
        uint64_t m_rttvar = 0; // 표준 편차

    public:
        uint64_t RTO(uint64_t rtt) {
            if (m_srtt == 0) {
                // 초기값 설정
                m_srtt = rtt;
                m_rttvar = rtt >> 1;
            }
            else {
                const uint64_t difference = m_srtt > rtt ? m_srtt - rtt : rtt - m_srtt;
                m_rttvar = m_rttvar - (m_rttvar >> 2) + (difference >> 2);
                m_srtt = m_srtt - (m_srtt >> 3) + (rtt >> 3);
            }
            return m_srtt + (m_rttvar << 2);
        }
    };
}