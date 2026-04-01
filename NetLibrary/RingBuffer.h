#pragma once

#include <cstdint>
#include <vector>
#include "Config.h"

namespace Net {
    constexpr uint8_t RELEASE = 0;
    constexpr uint8_t ACQUIRE = 1;

    struct BufferFragment
    {
        uint16_t front;
        uint16_t rear;
        uint16_t length;
        uint8_t* startPtr;
    };

    class RingBuffer {
        uint16_t m_head = 0;  // 읽기 포인터 (데이터 시작 위치)
        uint16_t m_tail = 0; // 쓰기 포인터 (빈 공간 시작 위치)
        bool m_last_op = RELEASE; // head == tail 일 때 버퍼 상태 구분 (false 읽기, true 쓰기)

        // 수신 버퍼 재사용, 메모리 복사 없이 패킷 사용하기 위함 (Wrap-around 구간제외 하면 연속적인 메모리 공간, 별도 처리 필요)
        std::vector<uint8_t> m_buffer;

        uint16_t HasSpace() const;
        
        void Initialize() {
            m_buffer.resize(RING_BUFFER_SIZE);
            m_last_op = RELEASE;
        }
        friend class ClientContext;
    public:
        uint16_t TryAcquireBuffer(BufferFragment& res);
        bool Release(uint16_t front, uint16_t rear);
        void ReleaseLeftOver(uint16_t p, bool hasData);
        uint8_t* GetStartPtr();
        uint16_t GetCapacity() const { return RING_BUFFER_SIZE; }
        void Clear() {
            m_head = 0;
            m_tail = 0;
            m_last_op = RELEASE;
        }
    };
}
