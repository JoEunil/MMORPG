#include "pch.h"
#include "RingBuffer.h"

#include <algorithm>

namespace Net {
	int16_t RingBuffer::HasSpace() const
	{
		if (m_tail == m_head)
			return m_last_op == RELEASE ? RECV_BUFFER_SIZE : 0;
		if (m_tail < m_head)
			return std::min<int16_t>(static_cast<int16_t>(RECV_BUFFER_SIZE), static_cast<int16_t>(m_head - m_tail));
		return std::min<int16_t>(static_cast<int16_t>(RECV_BUFFER_SIZE), static_cast<int16_t>(m_capacity - m_tail));
	}

	int16_t RingBuffer::TryAcquireBuffer(BufferFragment& res)
	{
		int16_t len = HasSpace();
		res.startPtr = m_buffer.data();
		res.front = m_tail;
		res.rear = m_tail + len - 1;
		res.rear %= m_capacity;
		m_tail = res.rear + 1;
		m_tail %= m_capacity;
		m_last_op = ACQUIRE;
		return len;
	}


	bool RingBuffer::Release(int16_t front, int16_t rear)
	{
		// 사용 완료한 버퍼 처리
		if (front != m_head)
			return false;
		m_head = rear + 1;
		m_head %= m_capacity;
		m_last_op = RELEASE;
		return true;
	}

	void RingBuffer::ReleaseLeftOver(int16_t notWr)
	{
		// 수신 후 남은 버퍼 공간만큼 앞으로 당기기
		m_tail = notWr;
		m_tail %= m_capacity;
		m_last_op = RELEASE;
	}

	uint8_t* RingBuffer::GetStartPtr() {
		return  m_buffer.data();
	}
}
