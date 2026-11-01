#include "pch.h"
#include "RingBuffer.h"

#include <algorithm>
namespace Net {
	uint16_t RingBuffer::AdvancePosition(uint16_t origin, uint16_t change) const {
		origin += change;
		return origin & m_mask;
	}
	uint16_t RingBuffer::HasSpace() const
	{
		if (m_tail == m_head)
			return m_last_op == RELEASE ? std::min<uint16_t>(m_capacity, m_capacity - m_tail) : 0;
		if (m_tail < m_head)
			return std::min<uint16_t>(static_cast<uint16_t>(m_capacity), static_cast<uint16_t>(m_head - m_tail));
		return std::min<uint16_t>(static_cast<uint16_t>(m_capacity), static_cast<uint16_t>(m_capacity - m_tail));
	}

	uint16_t RingBuffer::TryAcquireBuffer(BufferFragment& res, uint16_t fragmentSize)
	{
		uint16_t len = std::min(HasSpace(), fragmentSize);
		res.startPtr = m_buffer.data() + m_tail;
		res.front = m_tail;
		res.rear = AdvancePosition(m_tail, len - 1);
		res.length = len;
		if (len == 0)
			return 0; 
		m_tail = AdvancePosition(res.rear, 1);
		m_last_op = ACQUIRE;
		return len;
	}


	bool RingBuffer::Release(uint16_t front, uint16_t rear)
	{
		// 사용 완료한 버퍼 처리
		if (front != m_head)
			return false;
		uint16_t used;
		if (m_tail == m_head)
			used = (m_last_op == ACQUIRE) ? m_capacity : 0;
		else
			used = (m_tail > m_head) ? (m_tail - m_head) : (m_capacity - m_head + m_tail);

		uint16_t releaseLen = ((rear - front) & m_mask) + 1;
		if (releaseLen > used)
			return false;

		m_head = AdvancePosition(rear, 1);
		m_last_op = RELEASE;
		return true;
	}

	void RingBuffer::ReleaseLeftOver(uint16_t notWr, bool hasData)
	{
		// 수신 후 남은 버퍼 공간만큼 앞으로 당기기
		m_tail = AdvancePosition(notWr, 0);
		if (m_tail == m_head)
			m_last_op = hasData ? ACQUIRE : RELEASE;
	}

	uint8_t* RingBuffer::GetStartPtr(){
		return  m_buffer.data();
	}
}
