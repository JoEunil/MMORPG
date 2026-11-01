#include <gtest/gtest.h>

#include <cstdint>

#include <NetLibrary/PacketView.h>
#include <NetLibrary/ClientContext.h>

class PacketViewTest : public ::testing::Test {
protected:
	PacketViewTest() {
		for (int i = 0; i < 10; ++i) {
			packetViewPool.push_back(new Net::PacketView());
		}
	}
	~PacketViewTest() {
		for (auto pv : packetViewPool) {
			delete pv;
		}
	}
	std::vector<Net::PacketView*> packetViewPool;

	Net::PacketView* Getter() {
		if (packetViewPool.empty()) {
			return new Net::PacketView();
		}
		Net::PacketView* res = packetViewPool.back();
		packetViewPool.pop_back();
		return res;
	}
};

class ClientContextTest : public Net::ClientContext {
public:
	Net::PacketView* returned;
	~ClientContextTest() {
		if (returned) {
			delete returned;
		}
	}
	void ReleaseBuffer(Net::PacketView* pv) override {
		returned = pv;
	}
};

TEST_F(PacketViewTest, InitStatus) {
	Net::PacketView* pv = Getter();
	EXPECT_FALSE(pv->IsCopied());
	EXPECT_EQ(pv->GetPtr(), nullptr);
}

TEST_F(PacketViewTest, SetterGetter) {
	Net::PacketView* pv = Getter();
	Net::PacketBufferInfo info{ 3, 11, 15};
	std::vector<uint8_t> buffer(30);
	pv->SetBufferInfo(info, &buffer[0]);
	Net::PacketBufferInfo retrievedInfo = pv->GetBufferInfo();
	EXPECT_EQ(retrievedInfo.seq, 3);
	EXPECT_EQ(retrievedInfo.front,11);
	EXPECT_EQ(retrievedInfo.rear, 15);
	EXPECT_EQ(pv->GetLength(), 5);

	pv->SetSessionId(111);
	EXPECT_EQ(111, pv->GetSessionID());
	EXPECT_EQ(&buffer[11], pv->GetPtr());
}

TEST_F(PacketViewTest, ReleaseToOwner) {
	Net::PacketView* pv = Getter();
	Net::PacketBufferInfo info{ 3, 11, 15 };
	std::vector<uint8_t> buffer(30);
	pv->SetBufferInfo(info, &buffer[0]);

	ClientContextTest* owner = new ClientContextTest();
	pv->SetOwner(owner);
	pv->Release();
	Net::PacketBufferInfo retrievedInfo = owner->returned->GetBufferInfo();
	EXPECT_EQ(retrievedInfo.seq, 3);
	EXPECT_EQ(retrievedInfo.front, 11);
	EXPECT_EQ(retrievedInfo.rear, 15);

	EXPECT_EQ(owner->returned->GetLength(), 5);
	
	delete owner;
}

TEST_F(PacketViewTest, JoinBuffer) {
	std::vector<uint8_t> data(Net::RING_BUFFER_SIZE, '0');
	uint16_t front = Net::RING_BUFFER_SIZE - 3;
	uint16_t rear = 1;
	data[front] = '1';
	data[front+1] = '2';
	data[front + 2] = '3';
	data[0] = '4';
	data[1] = '5';
	Net::PacketView* pv = Getter();
	Net::PacketBufferInfo info{ 3, front, rear };
	pv->SetBufferInfo(info, reinterpret_cast<uint8_t*>(& data[0]));
	uint8_t* ptr = pv->GetPtr();
	uint16_t len = pv->GetLength();

	ASSERT_EQ(len, 5);
	EXPECT_TRUE(strncmp(reinterpret_cast<char*>(ptr), "12345", len) == 0);
}

