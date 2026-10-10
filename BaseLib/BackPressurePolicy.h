#pragma once

#include <cstdint>

namespace Base {
	enum class Priority : uint8_t {
		Droppable = 0,
		Important = 1
	};
}
