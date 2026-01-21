#pragma once

#include <mutex>
#include <cstdint>

#include <CoreLib/PacketTypes.h>

#include "RingBuffer.h"
#include "NetPacketFilter.h"
#include "Config.h"
#include "TrafficFloodDetector.h"

namespace Core {
    class IPacketDispatcher;
}

namespace Net {
    inline const uint16_t RELEASE_Q_SIZE = RING_BUFFER_SIZE / sizeof(Core::PacketHeader) + 2;
    // 링버퍼에서 최대로 사용할 수 있는 패킷
    inline const uint16_t EMPTY_SLOT = RING_BUFFER_SIZE;
    inline const std::pair<uint16_t, uint16_t> EMPTY_PAIR{ EMPTY_SLOT, EMPTY_SLOT };
    class PacketView;
    class ClientContext {
        uint32_t m_seq = 0;

        RingBuffer m_buffer;
        uint8_t* m_startPtr;
        uint16_t m_front = 0;
        uint16_t m_rear = m_capacity - 1;
        bool m_last_op = RELEASE;
        uint16_t m_capacity = 0;
        
        SOCKET m_sock;
        
        std::atomic<uint8_t> m_sessionPingCount; // 누적 시 종료 (half-open)
        std::atomic<uint64_t> m_rtt;

        uint64_t m_sessionID = 0;
        // 네트워크(소켓) -> 게임 로직(세션)

        std::atomic<bool> m_connected = false;
        std::atomic<int16_t> m_workingCnt = int16_t(0); // buffer 조각(패킷)을 점유하고 있는 작업의 수
        std::atomic<bool> m_flood = false;
        TrafficFloodDetector m_floodDetector;
        std::atomic<bool> m_gameSession = true;
        // recv가 한번씩 발생하고, recv 완료 후 다음 recv 호출이 이루어지기 때문에 atomic 플래그 하나로 충분
        // 경합 상황이면 flag 상태를 여러개로 나누고, CAS 함수가 필요했을 것.
        // 다음 recv에서 flood가 무조건 걸리도록 release, acquire만 잘 걸어주면 됨.

        uint16_t m_releaseIdx = 0;
        std::vector<std::pair<uint16_t, uint16_t>> m_releaseQ;
        // sequence % RELEASE_Q_SIZE를 index로 사용해서 ring Queue로 사용

        mutable std::mutex m_mutex;

        uint16_t GetLen();
        std::tuple<uint16_t, uint16_t, uint8_t> ParseHeader();
        bool DequeueRecvQ();
        void EnqueueReleaseQ(uint32_t seq, uint16_t front, uint16_t rear);

    public:
        ClientContext() {
            m_buffer.Initialize();
            m_capacity = RING_BUFFER_SIZE;
            m_startPtr = m_buffer.GetStartPtr();
            m_releaseQ.resize(RELEASE_Q_SIZE, { EMPTY_SLOT, EMPTY_SLOT });
            m_rear = m_capacity - 1;
            m_last_op = RELEASE;
        }
        uint16_t AllocateRecvBuffer(uint8_t*& buffer);

        void ReleaseBuffer(PacketView* pv);
        bool EnqueueRecvQ(uint8_t* ptr, size_t len);
        uint16_t GetWorkingCnt() const { return m_workingCnt.load(); }
        uint64_t GetSessionID() const { return m_sessionID; }
        SOCKET GetSocket() const { return m_sock; }
        void Clear(SOCKET s, uint64_t session) {
            m_sock = s;
            m_connected.store(true);
            m_sessionID = session;
            m_buffer.Clear();
            m_startPtr = m_buffer.GetStartPtr();
            m_workingCnt.store(0);
            m_seq = 0;
            m_front = 0;
            m_rear = m_capacity - 1;
        }
        void Disconnect() {
            m_connected.store(false);
            if (!m_connected.load() && m_workingCnt.load() <= 0) {
                NetPacketFilter::Disconnect(m_sessionID);
            }
        }

        uint8_t SendPing(std::chrono::steady_clock::time_point now) {
            NetPacketFilter::Ping(m_sessionID, m_rtt.load(std::memory_order_relaxed), now);
            return m_sessionPingCount.fetch_add(1) + 1;
        }
        void PongReceived() {
            m_sessionPingCount.store(0);
        }
        
        bool NetStatus() {
            if (m_connected.load() == false)
                return false;
            if (m_sessionPingCount.load(std::memory_order_relaxed) > PING_COUNT_LIMIT)
                return false;
            return true;
        }
        
        uint64_t GetRtt() {
            // race condition 안중요함, 틀려도 되는 데이터
            return m_rtt.load(std::memory_order_relaxed);
        }
        void SetRtt(uint64_t rtt) {
            m_rtt.store(rtt, std::memory_order_relaxed);
        }

        bool FloodCheck() {
            return m_flood.load(std::memory_order_acquire);
        }

        bool CheckGameSession() {
            return m_gameSession.load(std::memory_order_acquire);
        }
    };
}
