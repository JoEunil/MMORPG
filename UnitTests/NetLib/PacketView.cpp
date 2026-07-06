#include <gtest/gtest.h>

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
	pv->SetFront(11);
	EXPECT_EQ(pv->GetFront(), 11);
	pv->SetRear(15);
	EXPECT_EQ(pv->GetRear(), 15);
	EXPECT_EQ(pv->GetLength(), 5);
	pv->SetSeq(3);
	EXPECT_EQ(pv->GetSeq(), 3);

	pv->SetSessionId(111);
	EXPECT_EQ(111, pv->GetSessionID());
	uint8_t* ptr = new uint8_t[30];
	pv->SetStartPtr(ptr);
	EXPECT_EQ(ptr+pv->GetFront(), pv->GetPtr());
	delete[] ptr;

}

TEST_F(PacketViewTest, ReleaseToOwner) {
	Net::PacketView* pv = Getter();

	pv->SetFront(11);
	EXPECT_EQ(pv->GetFront(), 11);
	pv->SetRear(15);
	EXPECT_EQ(pv->GetRear(), 15);
	EXPECT_EQ(pv->GetLength(), 5);
	pv->SetSeq(3);
	EXPECT_EQ(pv->GetSeq(), 3);

	ClientContextTest* owner = new ClientContextTest();
	pv->SetOwner(owner);
	pv->Release();

	EXPECT_EQ(owner->returned->GetFront(), 11);
	EXPECT_EQ(owner->returned->GetRear(), 15);
	EXPECT_EQ(owner->returned->GetLength(), 5);
	EXPECT_EQ(owner->returned->GetSeq(), 3);
	
	delete owner;
}

TEST_F(PacketViewTest, JoinBuffer) {
	char data1[] = "12";
	char data2[] = "345";
	uint8_t* buffer1 = reinterpret_cast<uint8_t*>(data1);
	uint8_t* buffer2 = reinterpret_cast<uint8_t*>(data2);
	char joined[] = "12345";

	Net::PacketView* pv = Getter();
	pv->JoinBuffer(buffer1, 2, buffer2, 3);
	EXPECT_EQ(pv->GetLength(), 5);
	EXPECT_TRUE(std::strcmp(reinterpret_cast<char*>(pv->GetPtr()), joined) == 0);
	EXPECT_TRUE(pv->IsCopied());
	EXPECT_NE(pv->GetPtr(), buffer1);
	EXPECT_NE(pv->GetPtr(), buffer2);
}

