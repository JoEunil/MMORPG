#include "pch.h"
#include "PacketView.h"

namespace Net {
	void PacketView:: Release() {
		if (owner)
			owner->ReleaseBuffer(this);
	}
}
