#pragma once

#include <cstdint>
#include <vector>
#include "Config.h"
class RingBufferTest;

namespace Net {

    struct BufferFragment;

    class RingBuffer {
        uint16_t m_head = 0;  // 읽기 포인터 (데이터 시작 위치)
        uint16_t m_tail = 0; // 쓰기 포인터 (빈 공간 시작 위치)
        bool m_last_op = RELEASE; // head == tail 일 때 버퍼 상태 구분 (false 읽기, true 쓰기)

        // 수신 버퍼 재사용, 메모리 복사 없이 패킷 사용하기 위함 (Wrap-around 구간제외 하면 연속적인 메모리 공간, 별도 처리 필요)
        std::vector<uint8_t> m_buffer;
        const uint32_t m_capacity;
        const uint32_t m_mask;
        uint16_t HasSpace() const;
        uint16_t AdvancePosition(uint16_t origin, uint16_t change) const;

        friend class ::RingBufferTest;
    public:
        RingBuffer(uint32_t size) : m_capacity(size), m_mask(size - 1) {
            m_buffer.resize(size);
        }
        uint16_t TryAcquireBuffer(BufferFragment& res, uint16_t fragmentSize);
        bool Release(uint16_t front, uint16_t rear);
        void ReleaseLeftOver(uint16_t p, bool hasData);
        uint8_t* GetStartPtr();
        uint16_t GetCapacity() const { return m_capacity; }
        void Clear() {
            m_head = 0;
            m_tail = 0;
            m_last_op = RELEASE;
        }
    };

    struct BufferFragment
    {
        uint16_t front;
        uint16_t rear;
        uint16_t length;
        uint8_t* startPtr;
    };
}
