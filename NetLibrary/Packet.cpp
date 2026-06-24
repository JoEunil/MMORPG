#include "pch.h"
#include "Packet.h"
#include "PacketPool.h"
namespace Net {
	void Packet::Release() {
		if (owner)
			owner->Return(this);
		
	}
}