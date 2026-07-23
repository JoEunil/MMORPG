#pragma once

#include <mutex>
#include <cstdint>
#include <new>

#include "RingBuffer.h"
#include "NetPacketFilter.h"
#include "TrafficFloodDetector.h"
#include "PacketView.h"
#include "OverlappedExPool.h"
#include "Config.h"

#include <CoreLib/PacketTypes.h>
#include <BaseLib/FixedObjectPool.h>
#include <BaseLib/RingQueue.h>


namespace Core {
    class IPacketDispatcher;
}

enum class EnqueueSendResult
{
    Ready,      // 큐가 비어있어 즉시 전송 가능
    Queued,     // pending으로 큐에 적재됨 
    QueueFull,   // 큐 가득참 
    Failed     // 실패
};

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

        // m_workingCnt는 패킷마다 fetch_add/sub 되므로 자주 read 되는 두 flag와 cache line 분리.
        std::atomic<bool> m_connected = false;
        std::atomic<bool> m_gameSession = false;

        alignas(std::hardware_destructive_interference_size) std::atomic<int16_t> m_workingCnt = int16_t(0); // buffer 조각(패킷)을 점유하고 있는 작업의 수
        char padding[std::hardware_destructive_interference_size - sizeof(uint16_t)];

        std::recursive_mutex m_mutex; 
        // TryDispatch 실패하는 경우, EnqueueReleaseQ에서 DeadLock을 방지하기 위함. 
        // 그 이외에는 recursive 하지 않게 사용하기 때문에 성능상 크게 문제 없음.
        uint16_t m_releaseIdx = 0;
        std::vector<std::pair<uint16_t, uint16_t>> m_releaseQ;
        // sequence % RELEASE_Q_SIZE를 index로 사용해서 ring Queue로 사용

        Base::RingQueue<STOverlappedEx*, SEND_QUEUE_SIZE> m_sendQueue;
        std::mutex m_sendMutex;
        bool m_sendPending = false;
        // WSA Send 중첩을 방지하기 위함

        inline static OverlappedExPool* overlappedExPool;
        inline static Base::FixedObjectPool<PacketView, PACKETVIEWPOOL_SIZE> packetViewPool;

        uint16_t GetLen();
        std::tuple<uint16_t, uint16_t, uint8_t> ParseHeader();
        bool DequeueRecvQ();
        void EnqueueReleaseQ(uint32_t seq, uint16_t front, uint16_t rear);

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
    public:
        ClientContext(){
            m_buffer.Initialize(RING_BUFFER_SIZE);
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
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                m_sessionID = session;
                m_buffer.Clear();
                m_startPtr = m_buffer.GetStartPtr();
                m_seq = 0;
                m_front = 0;
                m_rear = RING_BUFFER_SIZE - 1;
                m_connected.store(true, std::memory_order_seq_cst);
                m_workingCnt.store(0, std::memory_order_seq_cst);
                m_gameSession.store(true, std::memory_order_release);
            }
            {
                std::lock_guard<std::mutex> lock(m_sendMutex);
                while (!m_sendQueue.empty()) {
                    overlappedExPool->Return(m_sendQueue.pop());
                }
            }
        }
        void Disconnect() {
            m_connected.store(false, std::memory_order_seq_cst);
            if (!m_connected.load(std::memory_order_seq_cst) && m_workingCnt.load(std::memory_order_seq_cst) <= 0) {
                NetPacketFilter::Disconnect(m_sessionID);
            }
        }
        bool CheckGameSession() {
            return m_gameSession.load(std::memory_order_acquire);
        }

        // unit test에서 mock 주입 위해 virtual 선언
        virtual void ReleaseBuffer(PacketView* pv);
        bool EnqueueRecvQ(uint8_t* ptr, size_t len);

        EnqueueSendResult EnqueueSend(STOverlappedEx* work);
        STOverlappedEx* DequeueSend();
    };
}
