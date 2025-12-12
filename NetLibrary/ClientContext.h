#pragma once

#include <mutex>
#include <cstdint>

#include <CoreLib/PacketTypes.h>
#include <CoreLib/PacketDispatcher.h>

#include "RingBuffer.h"
#include "Config.h"

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
        uint64_t m_sessionID = 0;
        // 네트워크(소켓) -> 게임 로직(세션)

        std::atomic<bool> m_connected = false;
        std::atomic<int16_t> m_workingCnt = int16_t(0); // buffer 조각(패킷)을 점유하고 있는 작업의 수
        uint16_t m_releaseIdx = 0;
        std::vector<std::pair<uint16_t, uint16_t>> m_releaseQ;
        // sequence % RELEASE_Q_SIZE를 index로 사용해서 queue 처럼 사용

        mutable std::mutex m_mutex;
        inline static Core::IPacketDispatcher* packetDispatcher;

        static void Initialize(Core::IPacketDispatcher* p) {
            packetDispatcher = p;
        };
        static bool IsReady() {
            return packetDispatcher != nullptr;
        }

        uint16_t GetLen();
        std::tuple<uint16_t, uint16_t, uint8_t> ParseHeader();
        bool DequeueRecvQ();
        void EnqueueReleaseQ(uint32_t seq, uint16_t front, uint16_t rear);

        friend class Initializer;

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
            std::cout << "client context disconnect\n";
            m_connected.store(false);
            if (!m_connected.load() && m_workingCnt.load() <= 0) {
                packetDispatcher->Disconnect(m_sessionID);
            }
        }
    };
}
