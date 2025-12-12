#pragma once

#include <cstdint>
#include <vector>
#include "Config.h"

namespace Net {
    constexpr const uint8_t RELEASE = 0;
    constexpr const uint8_t ACQUIRE = 1;

    struct BufferFragment
    {
        int16_t front;
        int16_t rear;
        int16_t length;
        uint8_t* startPtr;
    };

    class RingBuffer {
        int16_t m_head = 0;  // 읽기 포인터 (데이터 시작 위치)
        int16_t m_tail = 0; // 쓰기 포인터 (빈 공간 시작 위치)
        uint16_t m_capacity = 0;
        bool m_last_op = RELEASE; // head == tail 일 때 버퍼 상태 구분 (false 읽기, true 쓰기)

        // 수신 버퍼 재사용, 메모리 복사 없이 패킷 사용하기 위함 (Wrap-around 구간제외 하면 연속적인 메모리 공간, 별도 처리 필요)
        std::vector<uint8_t> m_buffer;

        int16_t HasSpace() const;
        
        void Initialize() {
            m_buffer.resize(RING_BUFFER_SIZE);
            m_capacity = RING_BUFFER_SIZE;
            m_last_op = RELEASE;
        }
        friend class ClientContext;
    public:
        int16_t TryAcquireBuffer(BufferFragment& res);
        bool Release(int16_t front, int16_t rear);
        void ReleaseLeftOver(int16_t p);
        uint8_t* GetStartPtr();
        int16_t GetCapacity() const { return m_capacity; }
        void Clear() {
            m_head = 0;
            m_tail = 0;
            m_last_op = RELEASE;
        }
    };
}
