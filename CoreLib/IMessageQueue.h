#pragma once
#include <cstdint>

#include "Message.h"

namespace Core {
	class IMessageQueue {
	public:
		virtual ~IMessageQueue() = default;
		// 수신 큐에 들어갔으면 true. 실패(큐 정지/풀 고갈/push 실패)는 false로 알린다.
		// 호출자가 재시도할지 버릴지 판단해야 하므로 조용히 삼키지 않는다.
		virtual bool EnqueueMessage(Message* m) = 0; // receiver
	};
}