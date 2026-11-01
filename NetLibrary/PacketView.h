#pragma once

#include <vector>
#include <cstdint>
#include <CoreLib/IPacketView.h>
#include <CoreLib/PacketTypes.h>
#include "Config.h"

namespace Net {
	class ClientContext;
	struct PacketBufferInfo {
		uint32_t seq = 0;            // 패킷 순서, release에 사용
		uint16_t front = 0;
		uint16_t rear = 0;
	};
	class PacketView : public Core::IPacketView
	{
		uint64_t m_sessionId = 0;
		ClientContext* owner = nullptr;
		uint8_t m_opcode = 0;

		PacketBufferInfo m_bufferInfo{};
		uint8_t* m_startPtr = nullptr;      // 실제 데이터 포인터 (RingBuffer 내부 OR copiedBuffer)
		uint16_t m_bufLen = 0;

		bool m_isCopied = false;           // true이면 wrap-around 때문에 memcpy로 생성된 버퍼
		std::vector<uint8_t> m_copiedBuffer;

		void JoinBuffer(uint8_t* ptr1, uint16_t length1, uint8_t* ptr2, uint16_t length2) {
			m_copiedBuffer.clear();
			m_copiedBuffer.reserve(length1 + length2);

			m_copiedBuffer.insert(m_copiedBuffer.end(), ptr1, ptr1 + length1);
			m_copiedBuffer.insert(m_copiedBuffer.end(), ptr2, ptr2 + length2);
			m_isCopied = true;
			m_startPtr = m_copiedBuffer.data();
			m_bufLen = length1 + length2;
		}
	public:
		PacketView() {
			m_copiedBuffer.reserve(Core::MAX_REQ_PACKET_LEN);
		}

		PacketView(const PacketView&) = delete;            // 복사 생성자 금지
		PacketView& operator=(const PacketView&) = delete; // 복사 대입 금지
		PacketView(PacketView&& other) = delete;           // 이동 생성자 금지
		PacketView& operator=(PacketView&&) = delete;      // 이동 대입 금지

		void Release() override;
		void SetBufferInfo(const PacketBufferInfo& info, uint8_t* startPtr) {
			m_bufferInfo = info;
			if (info.rear < info.front) {
				JoinBuffer(startPtr + info.front, RING_BUFFER_SIZE - info.front, startPtr, info.rear + 1);
				return;
			}
			m_startPtr = startPtr;
			m_bufLen = info.rear - info.front + 1;
		}	
		void SetSessionId(uint64_t s) { m_sessionId = s; }
		void SetOpcode(uint8_t opcode) { m_opcode = opcode; }
		void SetOwner(ClientContext* o) { owner = o; }
		bool IsCopied() const { return m_isCopied; }

		PacketBufferInfo GetBufferInfo() const { return m_bufferInfo; }
		uint64_t GetSessionID() const override { return m_sessionId; }
		uint8_t* GetPtr() const override { 
			if (m_isCopied)
				return m_startPtr;
			return m_startPtr + m_bufferInfo.front; 
		}
		uint16_t GetLength() const override { return m_bufLen;}
		uint8_t GetOpcode() const override { return m_opcode; }
		void Clear() {
			m_isCopied = false;
			m_opcode = 0;
			m_sessionId = 0;
			m_bufferInfo = {};
			m_startPtr = nullptr;
			m_bufLen = 0;
			m_copiedBuffer.clear();
			owner = nullptr;
		}
	};
}
