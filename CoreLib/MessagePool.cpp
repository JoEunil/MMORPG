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

	void MessagePool::Return(Message* msg) {
		m_fixedPool.Deallocate(msg);
	}

}
