#include "pch.h"

#include "MessagePool.h"
#include "Message.h"
#include "Config.h"


namespace Core {
	Message* MessagePool::Acquire()
	{
		Message* msg = m_fixedPool.Allocate();
		return msg;
	}

	MessagePool::ScopedMessage MessagePool::AcquireScoped() {
		return ScopedMessage{ Acquire(), MessageDeleter{ this } };
	}

	void MessagePool::Return(Message* msg) {
		m_fixedPool.Deallocate(msg);
	}

}
