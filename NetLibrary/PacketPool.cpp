#include "pch.h"
#include "PacketPool.h"
#include "Packet.h"
#include "Config.h"

#include <CoreLib/IPacket.h>
#include <CoreLib/LoggerGlobal.h>

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
		m_packets.reserve(m_maxPool);
        for (int i = 0; i < m_targetPool; i++)
        {
            m_packets.emplace_back(new Packet(m_packetLen, this));
        };
    }

	void PacketPool::Adjust()
	{
		uint32_t current = m_packets.size();

		if (current >= m_maxPool) {
			Decrease(current);
		}
		if (current <= m_minPool) {
			Increase(current);
		}
	}

	void PacketPool::Increase(uint32_t& size) {
		auto current = m_packets.size();

		if (current <= m_minPool) {
			while (current < m_targetPool)
			{
				Packet* packet = new Packet(m_packetLen, this);
				m_packets.push_back(packet);
				current++;
			}
			Core::sysLogger->LogInfo("packet pool", "Pool increased");
		}
	}

	void PacketPool::Decrease(uint32_t& size) {
		auto current = m_packets.size();

		if (current >= m_maxPool) {
			while (current > m_targetPool)
			{
				Packet* temp = m_packets.back();
				m_packets.pop_back();
				delete temp;
				current--;
			}
			Core::sysLogger->LogInfo("packet pool", "Pool decreased");
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
		rawPacket->Clear();
		// 커스텀 deleter: delete 대신 PacketPool에 반환
		return std::shared_ptr<Core::IPacket>(rawPacket, [this](Packet* p) { this->Return(p); });
	}

	std::unique_ptr<Core::IPacket, Core::PacketDeleter> PacketPool::AcquireUnique()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_packets.empty()) {
			return nullptr;
		}
		Packet* rawPacket = m_packets.back();
		m_packets.pop_back();
		Adjust();

		return std::unique_ptr<Core::IPacket, Core::PacketDeleter>(static_cast<Core::IPacket*>(rawPacket));
	}

	void PacketPool::Return(Core::IPacket* packet) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_packets.push_back(static_cast<Packet*>(packet)); 
		Adjust();
	}

}
