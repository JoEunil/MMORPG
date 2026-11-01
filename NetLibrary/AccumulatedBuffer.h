#pragma once
#include <cstdint>

#include "Config.h"

namespace Net {
	class AccumulatedBuffer {
        const uint8_t* m_startPtr;
        uint16_t m_front = 0;
        uint16_t m_rear = RING_BUFFER_SIZE - 1;
        bool m_last_op = RELEASE;

        uint16_t AdvancePosition(uint16_t origin, uint16_t change) const;
    public: 
        AccumulatedBuffer(uint8_t* startPtr): m_startPtr(startPtr) {
        }

        void Clear() {
            m_front = 0;
            m_rear = RING_BUFFER_SIZE - 1;
            m_last_op = RELEASE;
        }

        uint16_t GetLen() const;

        void Pop(uint32_t len);
        uint16_t Push(uint32_t len);

        bool IsNextPos(uint8_t* pos);

        uint8_t* GetStartPtr() const { return const_cast<uint8_t*>(m_startPtr);}
        uint16_t GetFront() const { return m_front; }
        uint16_t GetRear() const { return m_rear; }
        uint8_t* GetFrontPtr() const { return const_cast<uint8_t*>(m_startPtr) + m_front; }
	};
}