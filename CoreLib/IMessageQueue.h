#pragma once
#include <cstdint>

#include "Message.h"

namespace Core {
	class IMessageQueue {
	public:
		virtual ~IMessageQueue() = default;
		virtual void EnqueueMessage(Message* m) = 0; // receiver
	};
}