#include "pch.h"
#include "OverlappedExPool.h"
#include "PacketPool.h"
#include <CoreLib/LoggerGlobal.h>

namespace Net {
	OverlappedExPool::~OverlappedExPool() {
		for (auto* buf : m_acceptBuffers)
			delete[] buf;
	}

	STOverlappedEx* OverlappedExPool::Acquire() {
		STOverlappedEx* res = m_fixedPool.Allocate();
		return res;
	}

	void OverlappedExPool::Return(STOverlappedEx* r) {
		r->Reset();
		m_fixedPool.Deallocate(r);
	}
}
