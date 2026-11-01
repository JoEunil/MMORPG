#include "pch.h"
#include "AccumulatedBuffer.h"

namespace Net {

    uint16_t AccumulatedBuffer::AdvancePosition(uint16_t origin, uint16_t change) const {
        origin += change;
        return origin & RING_BUFFER_SIZE_MASK;
    }

    uint16_t AccumulatedBuffer::GetLen() const {
        if (m_last_op == RELEASE && m_front == AdvancePosition(m_rear, 1))
            return 0;
        if (m_front <= m_rear)
            return m_rear - m_front + 1;
        return RING_BUFFER_SIZE - m_front + m_rear + 1;
    }

    void AccumulatedBuffer::Pop(uint32_t len) {
        m_front = AdvancePosition(m_front, len);
        m_last_op = RELEASE;
    }

    uint16_t AccumulatedBuffer::Push(uint32_t len) {
        uint16_t rear = AdvancePosition(m_rear, len);
        m_rear = rear;
        m_last_op = ACQUIRE;
        return rear;
    }

    bool AccumulatedBuffer::IsNextPos(uint8_t* pos) {
        uint16_t front = pos - m_startPtr;
        if (front != AdvancePosition(m_rear, 1))
            return false;
        return true;
    }
}