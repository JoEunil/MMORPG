#include "pch.h"
#include "ClientContext.h"

#include "Config.h"

#include <CoreLib/PacketTypes.h>
#include <CoreLib/IPacketView.h>
#include <CoreLib/Config.h>
#include <CoreLib/LoggerGlobal.h>


namespace Net {
    uint16_t ClientContext::GetLen()
    {
        if (m_last_op == RELEASE && m_front == ((m_rear+1)&RING_BUFFER_SIZE_MASK))
            return 0;
        if (m_front <= m_rear)
            return m_rear - m_front + 1;
        return RING_BUFFER_SIZE - m_front + m_rear + 1;
    }

    std::tuple<uint16_t, uint16_t, uint8_t> ClientContext::ParseHeader() {
        uint8_t tempBuffer[sizeof(Core::PacketHeader)];
        uint8_t* bufferPtr;
        Core::PacketHeader* h;
        if (m_rear < m_front and RING_BUFFER_SIZE - m_front < sizeof(Core::PacketHeader)) {
            int firstPart = RING_BUFFER_SIZE - m_front;
            int secondPart = sizeof(Core::PacketHeader) - firstPart;

            std::memcpy(tempBuffer, m_startPtr + m_front, firstPart);
            std::memcpy(tempBuffer + firstPart, m_startPtr, secondPart);
            h = reinterpret_cast<Core::PacketHeader*>(&tempBuffer);
            if (h->magic != Core::MAGIC)
            {
                Core::gameLogger->LogWarn("context", "1 packet Magic invalid in header parsing", "session", m_sessionID, "magic", h->magic, "packetLen", h->length, "opcode", h->opcode, "m_front", m_front, "m_rear", m_rear, "GetLen()", GetLen());
            }
        }
        else {
            bufferPtr = m_startPtr + m_front;
            h = reinterpret_cast<Core::PacketHeader*>(bufferPtr);
            if (h->magic != Core::MAGIC)
            {
                Core::gameLogger->LogWarn("context", "2 packet Magic invalid in header parsing", "session", m_sessionID, "magic", h->magic, "packetLen", h->length, "opcode", h->opcode, "m_front", m_front, "m_rear", m_rear, "GetLen()", GetLen());
			}
        }
        // header가 wrap오버구간인 경우 처리
        return {h->magic, h->length, h->opcode };
    }

    bool ClientContext::DequeueRecvQ() {
        if (GetLen() < sizeof(Core::PacketHeader))
            return false;
        auto [magic, packetLen, opcode] = ParseHeader();
        // 2차 패킷 검증

        if (magic != Core::MAGIC)
        {
            Core::gameLogger->LogWarn("context", "packet Magic invalid", "session", m_sessionID, "magic", magic, "packetLen", packetLen, "opcode", opcode, "m_front", m_front, "m_rear", m_rear, "GetLen()", GetLen());
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }

        if (opcode == 0 or opcode > Core::MAX_DEFINED_OPCODE)
        {
            Core::gameLogger->LogWarn("context", "undefined opcode", "session", m_sessionID, "magic", magic, "packetLen", packetLen, "opcode", opcode, "m_front", m_front, "GetLen()", GetLen());
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }

        if (GetLen() < packetLen)
            return false;


        PacketView* packet = packetViewPool.Allocate();
        if (!packet) {
            Core::errorLogger->LogWarn("context", "packetViewPool empty");
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }
        packet->Clear();
        if (RING_BUFFER_SIZE - m_front < packetLen)
        {
            int firstPart = RING_BUFFER_SIZE - m_front;
            int secondPart = packetLen - firstPart;
            packet->JoinBuffer(m_startPtr + m_front, firstPart, m_startPtr, secondPart);
        }
        else {
            packet->SetStartPtr(m_startPtr);
        }

        packet->SetSessionId(m_sessionID);
        packet->SetSeq(m_seq++);
        packet->SetFront(m_front);
        packet->SetRear((m_front + packetLen - 1) & RING_BUFFER_SIZE_MASK);
        packet->SetOpcode(opcode);
        packet->SetOwner(this);

       std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter> pv(static_cast<Core::IPacketView*>(packet), Core::PacketViewDeleter{});

        m_front = (m_front + packetLen) & RING_BUFFER_SIZE_MASK;
        m_last_op = RELEASE;
        m_workingCnt.fetch_add(1, std::memory_order_seq_cst);

        if (!NetPacketFilter::TryDispatch(std::move(pv), m_srtt, m_rttvar)) {
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }
        return true;
    }
    uint16_t ClientContext::BeginRecvIO(uint64_t expectedSessionID, uint8_t*& buffer) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_connected.load(std::memory_order_seq_cst) || m_sessionID != expectedSessionID) {
            buffer = nullptr;
            return 0;
        }

        BufferFragment temp;
        uint16_t len = m_buffer.TryAcquireBuffer(temp, RECV_BUFFER_SIZE);
        buffer = temp.startPtr;
        if (len == 0) {
            Core::errorLogger->LogError("context", "can't allocate pending recv buffer", "sessionID", m_sessionID, "front", m_front, "rear", m_rear);
            return 0;
        }

        // shard lock 아래에서 alive 확인 후 증가한다. 이후 map에서 제거돼도
        // completion이 이 참조를 반납할 때까지 Context는 pool에서 재사용되지 않는다.
        m_pendingIOCnt.fetch_add(1, std::memory_order_seq_cst);
        return len;
    }

    void ClientContext::CompleteRecvIO(uint64_t expectedSessionID) {
        if (m_sessionID != expectedSessionID) {
            Core::errorLogger->LogError("context", "pending IO session mismatch", "expected", expectedSessionID, "actual", m_sessionID);
            return;
        }

        int16_t previous = m_pendingIOCnt.fetch_sub(1, std::memory_order_seq_cst);
        if (previous <= 0) {
            m_pendingIOCnt.fetch_add(1, std::memory_order_seq_cst);
            Core::errorLogger->LogError("context", "pending IO counter underflow", "sessionID", expectedSessionID, "previous", previous);
            return;
        }
    }


    bool ClientContext::EnqueueRecvQ(uint64_t expectedSessionID, uint8_t* ptr, size_t len) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_connected.load(std::memory_order_seq_cst) || m_sessionID != expectedSessionID)
            return false;
        uint16_t front = ptr - m_startPtr;
        if (front != ((m_rear + 1) & RING_BUFFER_SIZE_MASK))
            return false;
        uint16_t rear = m_rear + len;
        rear &= RING_BUFFER_SIZE_MASK;
        m_buffer.ReleaseLeftOver((rear + 1)& RING_BUFFER_SIZE_MASK, len != 0);
        m_rear = rear;

        m_last_op = ACQUIRE;
        while (DequeueRecvQ()) {}
        return true;
    }

    void ClientContext::EnqueueReleaseQ(uint32_t seq, uint16_t front, uint16_t rear) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_releaseQ[seq & RELEASE_Q_SIZE_MASK] = std::make_pair(front, rear);
        auto current = m_releaseQ[m_releaseIdx];
        while (current != EMPTY_PAIR and m_buffer.Release(current.first, current.second))
        {
            m_releaseQ[m_releaseIdx] = std::make_pair(EMPTY_SLOT, EMPTY_SLOT);
            m_releaseIdx++;
            m_releaseIdx &= RELEASE_Q_SIZE_MASK;
            current = m_releaseQ[m_releaseIdx];
        }
    }

    void ClientContext::ReleaseBuffer(PacketView* pv) {
        if (m_connected.load(std::memory_order_seq_cst))
            EnqueueReleaseQ(pv->GetSeq(), pv->GetFront(), pv->GetRear());
        int16_t previous = m_workingCnt.fetch_sub(1, std::memory_order_seq_cst);
        pv->Clear();
        packetViewPool.Deallocate(pv);
        if (previous == 1)
            TryNotifyDisconnect();
    }

    void ClientContext::TryNotifyDisconnect() {
        if (m_connected.load(std::memory_order_seq_cst) || m_workingCnt.load(std::memory_order_seq_cst) != 0)
            return;

        NetPacketFilter::Disconnect(m_sessionID);
    }
    EnqueueSendResult ClientContext::EnqueueSend(STOverlappedEx* work) {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        if (m_sendQueue.full())
            return EnqueueSendResult::QueueFull;
        if (m_sendPending == false) {
            m_sendPending = true;
            return EnqueueSendResult::Ready;
        }
        m_sendQueue.push(work);
        return EnqueueSendResult::Queued;
    }

    STOverlappedEx* ClientContext::DequeueSend() {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        if (m_sendQueue.empty()) {
            m_sendPending = false;
            return nullptr;
        }
        auto res = m_sendQueue.pop();
        return res;
    }
}
