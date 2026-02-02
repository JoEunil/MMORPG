#include "pch.h"
#include "PacketView.h"
#include "ClientContext.h"

namespace Net {
	void PacketView:: Release() {
		if (owner)
			owner->ReleaseBuffer(this);
	}
}