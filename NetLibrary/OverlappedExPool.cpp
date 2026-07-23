#include "pch.h"
#include "OverlappedExPool.h"
#include "PacketPool.h"
#include <CoreLib/LoggerGlobal.h>

namespace Net {
	OverlappedExPool::~OverlappedExPool() {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto* buf : m_acceptBuffers)
			delete[] buf;
	}


    void OverlappedExPool::Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
		for (int i = 0; i < PREPOSTED_ACCEPTS * 3; i++)
			m_acceptBuffers.emplace_back(new char[ACCEPT_BUFFER_SIZE]);
    }

	STOverlappedEx* OverlappedExPool::Acquire() {
		STOverlappedEx* res = m_fixedPool.Allocate();
		return res;
	}

	void OverlappedExPool::Return(STOverlappedEx* r) {
		r->originalBufs.clear();
		r->sharedPacket.reset();
		r->uniquePacket.reset();
		r->packetChunks.clear();
		r->wsaBuf.resize(1);
		m_fixedPool.Deallocate(r);
	}
}
