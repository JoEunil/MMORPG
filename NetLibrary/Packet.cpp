#include "pch.h"
#include "Packet.h"
namespace Net {
	void Packet::Release() {
		if (owner)
			owner->Return(this);
	}
}