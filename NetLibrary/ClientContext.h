#pragma once

#include <mutex>
#include <cstdint>
#include <new>
#include <optional>
#include <algorithm>

#include "RingBuffer.h"
#include "NetPacketFilter.h"
#include "TrafficFloodDetector.h"
#include "PacketView.h"
#include "OverlappedExPool.h"
#include "Jitter.h"
#include "AccumulatedBuffer.h"
#include "SendQueue.h"
#include "Config.h"

#include <CoreLib/PacketTypes.h>
#include <BaseLib/FixedObjectPool.h>
#include <BaseLib/RingQueue.h>


class ClientContextTest;
namespace Core {
    class IPacketDispatcher;
}

namespace Net {
    inline const uint16_t EMPTY_SLOT = RING_BUFFER_SIZE;
    inline const std::pair<uint16_t, uint16_t> EMPTY_PAIR{ EMPTY_SLOT, EMPTY_SLOT };
    class ClientContext {
        uint32_t m_seq = 0;

        RingBuffer m_buffer{ RING_BUFFER_SIZE };
        AccumulatedBuffer m_accBuf; 
        uint64_t m_sessionID = 0;
        
        Jitter m_jitter{};

        std::atomic<bool> m_connected = false;
        std::atomic<bool> m_gameSession = false;
        enum class DisconnectNotification : uint8_t { Waiting, Sending, Done };
        std::atomic<DisconnectNotification> m_disconnectNotification = DisconnectNotification::Waiting;
        // 서로 다른 실행 경로에서 갱신되는 작업 카운터 간 false sharing 방지.
        alignas(std::hardware_destructive_interference_size) std::atomic<int16_t> m_workingCnt = 0; // buffer 조각(패킷)을 점유하고 있는 작업의 수
        alignas(std::hardware_destructive_interference_size) std::atomic<int16_t> m_pendingIOCnt = 0; // Context 내부 버퍼를 참조하는 pending RECV 수
        char padding[std::hardware_destructive_interference_size - sizeof(std::atomic<int16_t>)];

        mutable std::recursive_mutex m_mutex;
        // TryDispatch 실패하는 경우, EnqueueReleaseQ에서 DeadLock을 방지하기 위함. 
        // 그 이외에는 recursive 하지 않게 사용하기 때문에 성능상 크게 문제 없음.
        uint16_t m_releaseIdx = 0;
        std::vector<std::pair<uint16_t, uint16_t>> m_releaseQ;
        // sequence % RELEASE_Q_SIZE를 index로 사용해서 ring Queue로 사용

		SendQueue m_sendQueue;

        inline static OverlappedExPool* overlappedExPool;
        inline static Base::FixedObjectPool<PacketView, PACKETVIEWPOOL_SIZE> packetViewPool;

        void BuildPacketView(PacketView* packet, uint32_t packetLen, uint8_t opcode);
        bool DequeueRecvQ(std::optional<uint64_t>& pongRTT);
        bool IsValidEnqueue(uint64_t expectedSessionID, uint8_t* ptr, size_t len);
        void EnqueueReleaseQ(PacketBufferInfo info);
        void TryNotifyDisconnect();
        uint16_t AdvancePosition(uint16_t origin, uint16_t change) const;

        static void Initialize(OverlappedExPool* o) {
            overlappedExPool = o;
        }
        static bool IsReady() {
            if (overlappedExPool == nullptr) {
                Core::errorLogger->LogError("context", "overlappedPool not Initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
        friend class ::ClientContextTest;
    public:
        ClientContext() : m_accBuf(m_buffer.GetStartPtr()){
            m_releaseQ.resize(RELEASE_Q_SIZE, { EMPTY_SLOT, EMPTY_SLOT });
        }
  

        uint16_t BeginRecvIO(uint64_t expectedSessionID, uint8_t*& buffer);
        void CompleteRecvIO(uint64_t expectedSessionID);
        uint16_t GetWorkingCnt() const { return m_workingCnt.load(std::memory_order_relaxed); }
        uint16_t GetPendingIOCnt() const { return m_pendingIOCnt.load(std::memory_order_relaxed); }
        uint64_t GetSessionID() const { return m_sessionID; }
        bool IsSessionAlive(uint64_t expectedSessionID) const {
            return m_connected.load(std::memory_order_acquire) && m_sessionID == expectedSessionID;
        }
        bool CanRecycle() const {
            // 마지막 PacketView 반납/종료 통지 스택이 완전히 빠져나오기 전에는
            // 재사용하지 않는다. pool mutex 아래에서 기다리지 않고 다음 flush로 넘긴다.
            std::unique_lock<std::recursive_mutex> lock(m_mutex, std::try_to_lock);
            if (!lock.owns_lock())
                return false;
            return !m_connected.load(std::memory_order_acquire)
                and m_disconnectNotification.load(std::memory_order_acquire) == DisconnectNotification::Done
                and m_workingCnt.load(std::memory_order_acquire) == 0
                and m_pendingIOCnt.load(std::memory_order_acquire) == 0;
        }

        void Clear(uint64_t session) {
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                m_sessionID = session;
                m_buffer.Clear();
                m_seq = 0;
                m_releaseIdx = 0;
                std::fill(m_releaseQ.begin(), m_releaseQ.end(), EMPTY_PAIR);
                m_disconnectNotification.store(DisconnectNotification::Waiting, std::memory_order_release);
                m_connected.store(true, std::memory_order_seq_cst);
                m_workingCnt.store(0, std::memory_order_seq_cst);
                m_pendingIOCnt.store(0, std::memory_order_seq_cst);
                m_gameSession.store(true, std::memory_order_release);
                m_jitter = Jitter{};
                m_accBuf.Clear();
            }
            m_sendQueue.Clear(overlappedExPool);
        }
        void Disconnect() {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            bool wasConnected = m_connected.exchange(false, std::memory_order_seq_cst);
            if (wasConnected)
                TryNotifyDisconnect();
        }
        bool CheckGameSession() const {
            return m_gameSession.load(std::memory_order_acquire);
        }
        
        void ReleaseBuffer(PacketView* pv);
        bool EnqueueRecvQ(uint64_t expectedSessionID, uint8_t* ptr, size_t len);

        EnqueueSendResult EnqueueSend(STOverlappedEx* work) {
			return m_sendQueue.Enqueue(work);
        }

        STOverlappedEx* DequeueSend() {
            return m_sendQueue.Dequeue();
        }
    };
}
