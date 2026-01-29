#include "pch.h"
#include "ClientContext.h"

#include "PacketView.h"
#include "Config.h"

#include <CoreLib/PacketTypes.h>
#include <CoreLib/IPacketView.h>
#include <CoreLib/Config.h>


namespace Net {
    uint16_t ClientContext::GetLen()
    {
        if (m_last_op == RELEASE && m_front == (m_rear+1)%m_capacity)
            return 0;
        if (m_front <= m_rear)
            return m_rear - m_front + 1;
        return m_capacity - m_front + m_rear + 1;
    }

    std::tuple<uint16_t, uint16_t, uint8_t> ClientContext::ParseHeader() {
        uint8_t tempBuffer[sizeof(Core::PacketHeader)];
        uint8_t* bufferPtr;
        Core::PacketHeader* h;
        if (m_rear < m_front and m_capacity - m_front < sizeof(Core::PacketHeader)) {
            int firstPart = m_capacity - m_front;
            int secondPart = sizeof(Core::PacketHeader) - firstPart;

            std::memcpy(tempBuffer, m_startPtr + m_front, firstPart);
            std::memcpy(tempBuffer + firstPart, m_startPtr, secondPart);
            h = reinterpret_cast<Core::PacketHeader*>(&tempBuffer);
        }
        else {
            bufferPtr = m_buffer.GetStartPtr() + m_front;
            h = reinterpret_cast<Core::PacketHeader*>(bufferPtr);
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
            std::cout << "Magic Error" << '\n';
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }

        if (opcode == 0 or opcode > Core::MAX_DEFINED_OPCODE)
        {
            std::cout << "undefined opcode" << '\n';
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }

        if (GetLen() < packetLen)
            return false;


        PacketView* packet = new PacketView;
        if (m_rear < m_front and m_capacity - m_front < packetLen)
        {
            int firstPart = m_capacity - m_front;
            int secondPart = packetLen - firstPart;
            packet->JoinBuffer(m_startPtr + m_front, firstPart, m_startPtr, secondPart);
        }
        else {
            packet->SetStartPtr(m_buffer.GetStartPtr());
        }

        packet->SetSessionId(m_sessionID);
        packet->SetSeq(m_seq++);
        packet->SetFront(m_front);
        packet->SetRear((m_front + packetLen - 1) % m_capacity);
        packet->SetOpcode(opcode);

        auto deleter = [this](Core::IPacketView* p) {this->ReleaseBuffer(static_cast<PacketView*>(p)); delete p; }; // lambda custom deleter
        std::shared_ptr<Core::IPacketView> pv(static_cast<Core::IPacketView*>(packet), deleter);

        m_front = (m_front + packetLen) % m_capacity;
        m_last_op = RELEASE;
        m_workingCnt.fetch_add(1);

        if (!NetPacketFilter::TryDispatch(pv)) {
            m_gameSession.store(false, std::memory_order_release);
            return false;
        }
        return true;
    }
    uint16_t ClientContext::AllocateRecvBuffer(uint8_t*& buffer) {
        BufferFragment temp;
        uint16_t len = m_buffer.TryAcquireBuffer(temp);
        buffer = temp.startPtr + temp.front;
        return len;
    }


    bool ClientContext::EnqueueRecvQ(uint8_t* ptr, size_t len) {
        uint16_t front = ptr - m_startPtr;
        if (front != (m_rear + 1) % m_capacity)
            return false;
        uint16_t rear = m_rear + len;
        rear %= m_capacity;
        m_buffer.ReleaseLeftOver(rear + 1);
        m_rear = rear;

        m_last_op = ACQUIRE;
        while (DequeueRecvQ()) {}
        return true;
    }

    void ClientContext::EnqueueReleaseQ(uint32_t seq, uint16_t front, uint16_t rear) {
        m_releaseQ[seq % RELEASE_Q_SIZE] = std::make_pair(front, rear);
        std::lock_guard<std::mutex> lock(m_releaseMutex);
        auto current = m_releaseQ[m_releaseIdx];
        while (current != EMPTY_PAIR and m_buffer.Release(current.first, current.second))
        {
            m_releaseQ[m_releaseIdx] = std::make_pair(EMPTY_SLOT, EMPTY_SLOT);
            m_releaseIdx++;
            m_releaseIdx %= RELEASE_Q_SIZE;
            current = m_releaseQ[m_releaseIdx];
        }
    }

    void ClientContext::ReleaseBuffer(PacketView* pv) {
        if (m_connected.load())
            EnqueueReleaseQ(pv->GetSeq(), pv->GetFront(), pv->GetRear());
        m_workingCnt.fetch_sub(1);

        if (!m_connected.load() && m_workingCnt.load() == 0) {
            NetPacketFilter::Disconnect(m_sessionID);
        }
    }
}
