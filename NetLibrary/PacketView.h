#pragma once

#include <vector>
#include <cstdint>
#include <CoreLib/IPacketView.h>

namespace Net {
	class PacketView : public Core::IPacketView
	{
		// 생성/소멸에 의해 병목이 생길 경우 PacketViewPool 사용.

		bool m_isCopied = false;           // true이면 wrap-around 때문에 memcpy로 생성된 버퍼
		uint8_t m_opcode;
		uint8_t* m_startPtr;      // 실제 데이터 포인터 (RingBuffer 내부 OR copiedBuffer)
		uint64_t m_sessionId;

		uint32_t m_seq;            // 패킷 순서, release에 사용
		int16_t m_front;
		int16_t m_rear;
		uint16_t m_length = 0;
		std::vector<uint8_t> m_copiedBuffer;

	public:
		void JoinBuffer(uint8_t* ptr1, uint16_t length1, uint8_t* ptr2, uint16_t length2) {
			m_copiedBuffer.clear();
			m_copiedBuffer.reserve(length1 + length2);

			m_copiedBuffer.insert(m_copiedBuffer.end(), ptr1, ptr1 + length1);
			m_copiedBuffer.insert(m_copiedBuffer.end(), ptr2, ptr2 + length2);
			m_isCopied = true;
			m_startPtr = m_copiedBuffer.data();
			m_length = length1 + length2;
		}

		void SetStartPtr(uint8_t* ptr) { m_startPtr = ptr; }
		void SetSessionId(uint64_t s) { m_sessionId = s; }
		void SetSeq(uint32_t seq) { m_seq = seq; }
		void SetFront(int16_t front) { m_front = front; }
		void SetRear(int16_t rear) { m_rear = rear; }
		void SetOpcode(uint8_t opcode) { m_opcode = opcode; }
		bool IsCopied() const { return m_isCopied; }

		uint32_t GetSeq() const { return m_seq; }
		int16_t GetFront() const { return m_front; }
		int16_t GetRear() const { return m_rear; }

		uint64_t GetSessionID() const override { return m_sessionId; }
		uint8_t* GetPtr() const override { 
			if (m_isCopied)
				return m_startPtr;
			return m_startPtr + m_front; 
		}
		uint16_t GetLength() const override {
			if (m_isCopied)
				return m_length;
			return m_rear - m_front + 1;
		}
		uint8_t GetOpcode() const override { return m_opcode; }
	};
}
