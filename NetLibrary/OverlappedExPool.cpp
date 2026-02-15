#include "pch.h"
#include "OverlappedExPool.h"
#include "PacketPool.h"
#include <CoreLib/LoggerGlobal.h>

namespace Net {
	OverlappedExPool::~OverlappedExPool() {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (STOverlappedEx* overlapped : m_overlappedPool) {
			delete overlapped;
		}
		m_overlappedPool.clear();
	}


    void OverlappedExPool::Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
		m_overlappedPool.reserve(MAX_OVERLAPPEDPOOL_SIZE);
		m_acceptBuffers.reserve(PREPOSTED_ACCEPTS*3);
        for (int i = 0; i < TARGET_OVERLAPPEDPOOL_SIZE; i++)
        {
            m_overlappedPool.emplace_back(new STOverlappedEx());
        }
		for (int i = 0; i < PREPOSTED_ACCEPTS * 3; i++)
			m_acceptBuffers.emplace_back(new char[ACCEPT_BUFFER_SIZE]);
    }
	void OverlappedExPool::Adjust()
	{
		size_t current = m_overlappedPool.size();
	
		if (current >= MAX_OVERLAPPEDPOOL_SIZE) {
			Decrease(current);
			Core::sysLogger->LogInfo("overlapped pool", "Pool increased");
		}
		if (current <= MIN_OVERLAPPEDPOOL_SIZE) {
			Increase(current);
			Core::sysLogger->LogInfo("overlapped pool", "Pool decreased");
		}
	}

	void OverlappedExPool::Increase(uint16_t currentSize) {
		while (currentSize < TARGET_OVERLAPPEDPOOL_SIZE)
		{
			STOverlappedEx* temp = new STOverlappedEx;
			m_overlappedPool.push_back(temp);
			currentSize++;
		}
	}

	void OverlappedExPool::Decrease(uint16_t currentSize) {
		while (currentSize > TARGET_OVERLAPPEDPOOL_SIZE)
		{
			STOverlappedEx* temp = m_overlappedPool.back();
			m_overlappedPool.pop_back();
			delete temp;
			currentSize--;
		}
	}

	STOverlappedEx* OverlappedExPool::Acquire() {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_overlappedPool.empty()) {
			return nullptr;
		}
		STOverlappedEx* res = m_overlappedPool.back();
		m_overlappedPool.pop_back();
		Adjust();
		return res;
	}

	void OverlappedExPool::Return(STOverlappedEx* r) {
		r->sharedPacket.reset();
		r->uniquePacket.reset();
		// nullptr도 안전
		std::lock_guard<std::mutex> lock(m_mutex);
		m_overlappedPool.push_back(r);
		Adjust();
	}
}
