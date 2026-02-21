#pragma once

#include <mutex>
#include <cstdint>

#include "RingBuffer.h"
#include "NetPacketFilter.h"
#include "TrafficFloodDetector.h"
#include "PacketView.h"
#include "Config.h"

#include <CoreLib/PacketTypes.h>
#include <BaseLib/ObjectPool.h>


namespace Core {
    class IPacketDispatcher;
}

namespace Net {
    inline const uint16_t EMPTY_SLOT = RING_BUFFER_SIZE;
    inline const std::pair<uint16_t, uint16_t> EMPTY_PAIR{ EMPTY_SLOT, EMPTY_SLOT };
    class ClientContext {
        uint32_t m_seq = 0;

        RingBuffer m_buffer;
        uint8_t* m_startPtr;
        uint16_t m_front = 0;
        uint16_t m_rear = RING_BUFFER_SIZE - 1;
        bool m_last_op = RELEASE;
        uint64_t m_sessionID = 0;

        std::atomic<bool> m_connected = false;
        std::atomic<int16_t> m_workingCnt = int16_t(0); // buffer 조각(패킷)을 점유하고 있는 작업의 수
        std::atomic<bool> m_gameSession = false;

        std::mutex m_releaseMutex;
        uint16_t m_releaseIdx = 0;
        std::vector<std::pair<uint16_t, uint16_t>> m_releaseQ;
        // sequence % RELEASE_Q_SIZE를 index로 사용해서 ring Queue로 사용

        inline static Base::ObjectPool<PacketView> packetViewPool{ TARGET_PACKETVIEWPOOL_SIZE, MAX_PACKETVIEWPOOL_SIZE, MIN_PACKETVIEWPOOL_SIZE };

        uint16_t GetLen();
        std::tuple<uint16_t, uint16_t, uint8_t> ParseHeader();
        bool DequeueRecvQ();
        void EnqueueReleaseQ(uint32_t seq, uint16_t front, uint16_t rear);

    public:
        ClientContext(){
            m_buffer.Initialize();
            m_startPtr = m_buffer.GetStartPtr();
            m_releaseQ.resize(RELEASE_Q_SIZE, { EMPTY_SLOT, EMPTY_SLOT });
            m_front = 0;
            m_rear = RING_BUFFER_SIZE - 1;
            m_last_op = RELEASE;
        }

        uint16_t AllocateRecvBuffer(uint8_t*& buffer);
        uint16_t GetWorkingCnt() const { return m_workingCnt.load(std::memory_order_relaxed); }
        uint64_t GetSessionID() const { return m_sessionID; }

        void Clear(uint64_t session) {
            m_sessionID = session;
            m_buffer.Clear();
            m_startPtr = m_buffer.GetStartPtr();
            m_seq = 0;
            m_front = 0;
            m_rear = RING_BUFFER_SIZE - 1;
            m_connected.store(true, std::memory_order_release);
            m_workingCnt.store(0, std::memory_order_release);
            m_gameSession.store(true, std::memory_order_release);
        }
        void Disconnect() {
            m_connected.store(false, std::memory_order_release);
            if (!m_connected.load(std::memory_order_acquire) && m_workingCnt.load(std::memory_order_acquire) <= 0) {
                NetPacketFilter::Disconnect(m_sessionID);
            }
        }
        bool CheckGameSession() {
            return m_gameSession.load(std::memory_order_acquire);
        }

        void ReleaseBuffer(PacketView* pv);
        bool EnqueueRecvQ(uint8_t* ptr, size_t len);
    };
}
