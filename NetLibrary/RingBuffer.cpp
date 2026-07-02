#include "pch.h"
#include "RingBuffer.h"

#include <algorithm>
namespace Net {
	uint16_t RingBuffer::HasSpace() const
	{
		if (m_tail == m_head)
			return m_last_op == RELEASE ? std::min<uint16_t>(m_size, m_size - m_tail) : 0;
		if (m_tail < m_head)
			return std::min<uint16_t>(static_cast<uint16_t>(m_size), static_cast<uint16_t>(m_head - m_tail));
		return std::min<uint16_t>(static_cast<uint16_t>(m_size), static_cast<uint16_t>(m_size - m_tail));
	}

	uint16_t RingBuffer::TryAcquireBuffer(BufferFragment& res, uint16_t fragmentSize)
	{
		uint16_t len = std::min(HasSpace(), fragmentSize);
		res.startPtr = m_buffer.data() + m_tail;
		res.front = m_tail;
		res.rear = m_tail + len - 1;
		res.rear &= m_mask;
		res.length = len;
		m_tail = res.rear + 1;
		m_tail &= m_mask;
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
			used = (m_last_op == ACQUIRE) ? m_size : 0;
		else
			used = (m_tail > m_head) ? (m_tail - m_head) : (m_size - m_head + m_tail);

		uint16_t releaseLen = ((rear - front) & m_mask) + 1;
		if (releaseLen > used)
			return false;

		m_head = rear + 1;
		m_head &= m_mask;
		m_last_op = RELEASE;
		return true;
	}

	void RingBuffer::ReleaseLeftOver(uint16_t notWr, bool hasData)
	{
		// 수신 후 남은 버퍼 공간만큼 앞으로 당기기
		m_tail = notWr;
		m_tail &= m_mask;
		if (m_tail == m_head)
			m_last_op = hasData ? ACQUIRE : RELEASE;
	}

	uint8_t* RingBuffer::GetStartPtr() {
		return  m_buffer.data();
	}
}
