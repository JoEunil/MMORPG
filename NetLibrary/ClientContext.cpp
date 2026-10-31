#include "pch.h"
#include "ClientContext.h"

#include "Config.h"

#include <CoreLib/PacketTypes.h>
#include <CoreLib/IPacketView.h>
#include <CoreLib/Config.h>
#include <CoreLib/LoggerGlobal.h>


namespace Net {
    Core::PacketHeader ClientContext::ParseHeader() const {
        Core::PacketHeader header;
        const size_t firstPart = (std::min)(sizeof(header), static_cast<size_t>(RING_BUFFER_SIZE - m_accBuf.GetFront()));
        std::memcpy(&header, m_accBuf.GetFrontPtr(), firstPart);
        if (firstPart < sizeof(header))
            std::memcpy(reinterpret_cast<uint8_t*>(&header) + firstPart, m_accBuf.GetStartPtr(), sizeof(header) - firstPart);
        return header; // wire의 uint32_t length를 검증 전에 축소하지 않는다.
    }

    bool ClientContext::CheckPacketValid(const Core::PacketHeader& header) const{
        const auto magic = header.magic;
        const uint32_t packetLen = header.length;
        const auto opcode = header.opcode;

        uint16_t front = m_accBuf.GetFront();
        uint16_t rear = m_accBuf.GetRear();
        uint16_t len = m_accBuf.GetLen();
        if (magic != Core::MAGIC) {
            Core::gameLogger->LogWarn("context", "packet Magic invalid", "session", m_sessionID, "magic", magic, "packetLen", packetLen, "opcode", opcode, "m_front", front, "m_rear", rear, "GetLen()", len);
            return false;
        }

        if (opcode == 0 || opcode > Core::MAX_DEFINED_OPCODE) {
            Core::gameLogger->LogWarn("context", "undefined opcode", "session", m_sessionID, "magic", magic, "packetLen", packetLen, "opcode", opcode, "m_front", front, "GetLen()", len);
            return false;
        }

        if (packetLen < sizeof(Core::PacketHeader) || packetLen > MAX_PACKET_LEN) {
            Core::gameLogger->LogWarn("context", "invalid packet length", "session", m_sessionID, "packetLen", packetLen, "opcode", opcode);
            return false;
        }
        // 고정 길이 요청이 짧은 본문이나 잘못된 dispatcher로 전달되지 않도록 한다.
        if (opcode == Core::OP::BAZAAR_SEARCH
            && (header.flags != 0 || packetLen != sizeof(Core::PacketStruct<Core::BazaarSearchRequestBody>)))
            return false;
        if (opcode == Core::OP::BAZAAR_MY_LIST
            && (header.flags != 0 || packetLen != sizeof(Core::PacketStruct<Core::BazaarMyListRequestBody>)))
            return false;
        if (opcode == Core::OP::ITEM_USE
            && (header.flags != 0 || packetLen != sizeof(Core::PacketStruct<Core::ItemUseRequestBody>)))
            return false;
        return true;
    }

    void ClientContext::BuildPacketView(PacketView* packet, uint32_t packetLen, uint8_t opcode) {
        packet->Clear();
        if (RING_BUFFER_SIZE - m_accBuf.GetFront() < packetLen)
        {
            int firstPart = RING_BUFFER_SIZE - m_accBuf.GetFront();
            int secondPart = packetLen - firstPart;
            packet->JoinBuffer(m_accBuf.GetStartPtr() + m_accBuf.GetFront(), firstPart, m_accBuf.GetStartPtr(), secondPart);
        }
        else {
            packet->SetStartPtr(m_accBuf.GetStartPtr());
        }

        packet->SetSessionId(m_sessionID);
        packet->SetSeq(m_seq++);
        packet->SetFront(m_accBuf.GetFront());
        packet->SetRear((m_accBuf.GetFront() + packetLen - 1) & RING_BUFFER_SIZE_MASK);
        packet->SetOpcode(opcode);
        packet->SetOwner(this);
    }

    bool ClientContext::DequeueRecvQ(std::optional<uint64_t>& pongRTT) {
        if (m_accBuf.GetLen() < sizeof(Core::PacketHeader))
            return false;
        const auto header = ParseHeader();
        const auto magic = header.magic;
        const uint32_t packetLen = header.length;
        const auto opcode = header.opcode;

        if (!CheckPacketValid(header)) {
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }
        if (m_accBuf.GetLen() < packetLen)
            return false;

        PacketView* packet = packetViewPool.Allocate();
        if (!packet) {
            Core::errorLogger->LogWarn("context", "packetViewPool empty");
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }

        BuildPacketView(packet, packetLen, opcode);
        std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter> pv(static_cast<Core::IPacketView*>(packet), Core::PacketViewDeleter{});

        m_accBuf.Pop(packetLen);
        m_workingCnt.fetch_add(1, std::memory_order_seq_cst);

        if (!NetPacketFilter::TryDispatch(std::move(pv), m_jitter, pongRTT)) {
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
            Core::errorLogger->LogError("context", "can't allocate pending recv buffer", "sessionID", m_sessionID, "front", m_accBuf.GetFront(), "rear", m_accBuf.GetRear());
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

    bool ClientContext::IsValidEnqueue(uint64_t expectedSessionID, uint8_t* ptr, size_t len) {
        if (!m_connected.load(std::memory_order_seq_cst) or m_sessionID != expectedSessionID)
            return false;
        if (!m_gameSession.load(std::memory_order_acquire) or len == 0 or len > RECV_BUFFER_SIZE)
            return false;
        if (!m_accBuf.IsNextPos(ptr))
            return false;
        return true;
    }

    bool ClientContext::EnqueueRecvQ(uint64_t expectedSessionID, uint8_t* ptr, size_t len) {
        std::unique_lock<std::recursive_mutex> lock(m_mutex);
        if (!IsValidEnqueue(expectedSessionID, ptr, len))
            return false;

        uint16_t rear = m_accBuf.Push(len);
        m_buffer.ReleaseLeftOver(rear + 1, len != 0);

        std::optional<uint64_t> pongRTT;
        while (DequeueRecvQ(pongRTT)) {}
        bool valid = m_gameSession.load(std::memory_order_acquire);
        lock.unlock();
        // Disconnect/BeginRecvIO는 shard -> Context 순서다. 수신 경로는
        // Context lock을 풀고 shard를 얻어 역순 잠금을 만들지 않는다.
        // 이 호출의 pending RECV 참조가 반환될 때까지 Context 재사용은 금지된다.
        if (pongRTT)
            NetPacketFilter::NotifyPong(expectedSessionID, *pongRTT);
        return valid;
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
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
        auto expected = DisconnectNotification::Waiting;
        if (!m_disconnectNotification.compare_exchange_strong(expected, DisconnectNotification::Sending,
            std::memory_order_acq_rel))
            return;
        // 마지막 PacketView가 workingCnt를 0으로 만든 뒤에도 종료 통지 완료 전에는
        // pool이 Clear로 새 session ID를 덮어쓰지 못하게 한다. 통지는 정확히 한 번이다.
        NetPacketFilter::Disconnect(m_sessionID);
        m_disconnectNotification.store(DisconnectNotification::Done, std::memory_order_release);
    }
    EnqueueSendResult ClientContext::EnqueueSend(STOverlappedEx* work) {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        if (m_sendPending == false) {
            m_sendPending = true;
            return EnqueueSendResult::Ready;
        }
        if (!m_sendQueue.push(work))
            return EnqueueSendResult::QueueFull;
        return EnqueueSendResult::Queued;
    }

    STOverlappedEx* ClientContext::DequeueSend() {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        STOverlappedEx* res = nullptr;
        if (!m_sendQueue.pop(res)) {
            m_sendPending = false;
            return nullptr;
        }
        return res;
    }
}
