#pragma once
#include <cstdint>
namespace Net {
	class IAbortSocket {
		// 강제로 socket close 하기위한 인터페이스
		// Cheat 감지나 Zomebie 클라이언트(half-open) 제거하기 위함 
	public:
		virtual void AbortSocket(SOCKET sock) = 0;
		// 이걸 호출 할 수 있는 부분은 socket 정보를 알고있는 부분이어야 함.
	};
}