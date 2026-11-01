#include <gtest/gtest.h>

#include <NetLibrary/ClientContext.h>
#include <CoreLib/PacketTypes.h>

Core::PacketStruct<Core::EnterWorldRequestBody> GetTestPacket() {
	Core::PacketStruct<Core::EnterWorldRequestBody> packet;
	packet.header.opcode = Core::OP::ENTER_WORLD;
	packet.header.flags = 1;
	packet.header.magic = Core::MAGIC;
	packet.header.length = sizeof(Core::PacketStruct<Core::EnterWorldRequestBody>);
	packet.body.characterID = 12345;
	return packet;
}
class ClientContextTest : public ::testing::Test {
public:
	Net::ClientContext context;
	ClientContextTest() {
		context.InitSession(12345);
	}
	uint16_t GetRecvBuffer(uint8_t*& buf) {
		return context.BeginRecvIO(12345, buf);
	}
	uint16_t GetAccBufLen() {
		return context.m_accBuf.GetLen();
	}
	uint32_t GetWorkingCnt() {
		return context.m_workingCnt.load(std::memory_order_seq_cst);
	}
	void Disconnect() {
		context.m_connected.store(false, std::memory_order_seq_cst);
	}
};

TEST_F(ClientContextTest, recvFragment) {
	auto p = GetTestPacket(); 
	uint8_t* buf;
	GetRecvBuffer(buf);
	std::memcpy(buf, &p, sizeof(p)-3);
	EXPECT_TRUE(context.EnqueueRecvQ(12345, buf, sizeof(p) - 3));
	EXPECT_EQ(GetAccBufLen(), p.header.length - 3);
	EXPECT_EQ(GetWorkingCnt(), 0);
}

TEST_F(ClientContextTest, Disconnect) {
	auto p = GetTestPacket();
	uint8_t* buf;
	GetRecvBuffer(buf);
	std::memcpy(buf, &p, sizeof(p));
	EXPECT_TRUE(context.EnqueueRecvQ(12345, buf, sizeof(p) - 1));
	Disconnect();
	EXPECT_FALSE(context.EnqueueRecvQ(12345, buf, sizeof(p) - 1));
	EXPECT_EQ(GetRecvBuffer(buf), 0);
}