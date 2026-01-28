#include "pch.h"
#include "PacketPool.h"
#include "Packet.h"
#include "Config.h"

#include <CoreLib/IPacket.h>

namespace Net {
	PacketPool::~PacketPool() {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (Packet* pkt : m_packets) {
			delete pkt;
		}
		m_packets.clear();
	}

    void PacketPool::Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i < m_targetPool; i++)
        {
            Packet* packet = new Packet(MAX_PACKET_LEN, this);
            m_packets.push_back(packet);
        };
    }

	void PacketPool::Adjust()
	{
		uint16_t current = m_packets.size();

		if (current > m_maxPool) {
			Decrease(current);
		}
		if (current < m_minPool) {
			Increase(current);
		}
	}

	void PacketPool::Increase(uint16_t& size) {
		auto current = m_packets.size();

		if (current < m_minPool) {
			while (current < m_targetPool)
			{
				Packet* packet = new Packet(MAX_PACKET_LEN, this);
				m_packets.push_back(packet);
				current++;
			}
		}
	}

	void PacketPool::Decrease(uint16_t& size) {
		auto current = m_packets.size();

		if (current > m_maxPool) {
			while (current > m_targetPool)
			{
				Packet* temp = m_packets.front();
				m_packets.pop_front();
				delete temp;
				current--;
			}
		}
	}

	std::shared_ptr<Core::IPacket> PacketPool::Acquire()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_packets.empty()) {
			return nullptr;
		}
		Packet* rawPacket = m_packets.back();
		m_packets.pop_back();
		Adjust();

		// 커스텀 deleter: delete 대신 PacketPool에 반환
		return std::shared_ptr<Packet>(rawPacket, [this](Packet* p) { this->Return(p); });
	}

	std::unique_ptr<Core::IPacket> PacketPool::AcquireUnique()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_packets.empty()) {
			return nullptr;
		}
		Packet* rawPacket = m_packets.back();
		m_packets.pop_back();
		Adjust();
		// unique_ptr은 커스텀 deleter 사용 복잡성과, std::function의 비용 때문에 직접 Return 호출해서 사용.
		return std::unique_ptr<Packet>(rawPacket);
	}

	void PacketPool::Return(Core::IPacket* packet) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_packets.push_back(static_cast<Packet*>(packet)); // 인터페이스에 순수 가상함수만 정의되어 static_cast는 안전함
		Adjust();
	}

}
