#pragma once

#include <cstdint>

namespace Base {
	enum class Priority : uint8_t {
		Droppable = 0,
		Important = 1
	};

    enum class EnqueueResult : uint8_t {
        Queued,              // Accepted by an execution queue; not yet processed.
        RecordedForRecovery, // Recorded durably, but not queued for execution.
        Rejected
    };
}
