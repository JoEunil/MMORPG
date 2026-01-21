#pragma once
#include <cstdint>
namespace Net {
	class IAbortSocket {
		// 강제로 socket close 하기위한 인터페이스
		// Cheat 감지나 Zomebie 클라이언트(half-open) 제거하기 위함 
	public:
		virtual void AbortSocket(uint64_t session) = 0;
	};
}