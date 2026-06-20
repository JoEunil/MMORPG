#include "MessagePool.h"
#include "Config.h"

#include <CoreLib/Message.h>

#include <memory>
#include <mutex>

namespace Cache {
    Core::Message* MessagePool::Acquire() {
		Core::Message* msg = m_fixedPool.Allocate();
        return msg;
    }

    void MessagePool::Return(Core::Message* msg) {
		m_fixedPool.Deallocate(msg);
    }

}
