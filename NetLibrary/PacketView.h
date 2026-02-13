#pragma once

#include <vector>
#include <cstdint>
#include <CoreLib/IPacketView.h>

namespace Net {
	class ClientContext;
	class PacketView : public Core::IPacketView
	{
		bool m_isCopied = false;           // true이면 wrap-around 때문에 memcpy로 생성된 버퍼
		uint8_t m_opcode;
		uint8_t* m_startPtr;      // 실제 데이터 포인터 (RingBuffer 내부 OR copiedBuffer)
		uint64_t m_sessionId;

		uint32_t m_seq;            // 패킷 순서, release에 사용
		int16_t m_front;
		int16_t m_rear;
		uint16_t m_length = 0;
		std::vector<uint8_t> m_copiedBuffer;
		ClientContext* owner;
	public:
		PacketView() {
			m_isCopied = false;
			m_opcode = 0;
			m_startPtr = nullptr;
			m_sessionId = 0;
			m_seq = 0;
			m_front = 0;
			m_rear = 0;
			m_length = 0;
			m_copiedBuffer.reserve(64);
			owner = nullptr;
		}

		PacketView(const PacketView&) = delete;            // 복사 생성자 금지
		PacketView& operator=(const PacketView&) = delete; // 복사 대입 금지
		PacketView(PacketView&& other) noexcept
			: m_isCopied(other.m_isCopied),
			m_opcode(other.m_opcode),
			m_startPtr(other.m_startPtr),
			m_sessionId(other.m_sessionId),
			m_seq(other.m_seq),
			m_front(other.m_front),
			m_rear(other.m_rear),
			m_length(other.m_length),
			m_copiedBuffer(std::move(other.m_copiedBuffer)),
			owner(other.owner)
		{
			// 원본 초기화
			other.m_isCopied = false;
			other.m_startPtr = nullptr;
			other.m_sessionId = 0;
			other.m_seq = 0;
			other.m_front = 0;
			other.m_rear = 0;
			other.m_length = 0;
			other.owner = nullptr;
		}

		void Release() override;
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
		void SetOwner(ClientContext* o) { owner = o; }
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
		void Clear() {
			m_isCopied = false;
			m_opcode = 0;
			m_startPtr = nullptr;
			m_sessionId = 0;
			m_seq = 0;
			m_front = 0;
			m_rear = 0;
			m_length = 0;
			m_copiedBuffer.clear();
			owner = nullptr;
		}
	};
}
