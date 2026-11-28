#pragma once
#include <BaseLib/BackPressurePolicy.h>

#include "IMessage.h"

namespace Core {
	class IMessageQueue {
	public:
		virtual ~IMessageQueue() = default;
        // Queued: 실행 큐 접수, RecordedForRecovery: 복구용 기록만 성공.
        // 호출자는 처리 완료와 구분하며, Rejected일 때 기존 실패 정책을 적용한다.
		virtual Base::EnqueueResult EnqueueMessage(const IMessage* m) = 0;
	};
}
