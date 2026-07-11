#include <gtest/gtest.h>

#include <BaseLib/WAL.h>

#pragma pack(push,1) 
struct WalRecordTest {
    uint64_t a;
    uint32_t b;
    uint16_t c;
    uint8_t  d;
};
#pragma pack(pop)

size_t RECORED_LEN = sizeof(WalRecordTest) + sizeof(uint32_t) + sizeof(uint32_t);

void ApplyEmpty(const WalRecordTest&) {
}

static unsigned long long replayCnt = 1;
void ApplyRecordCompare(const WalRecordTest& out) {
    EXPECT_EQ(out.a, replayCnt);
    EXPECT_EQ(out.b, replayCnt);
    EXPECT_EQ(out.c, replayCnt);
    EXPECT_EQ(out.d, replayCnt);    
    replayCnt++;
}

TEST(WALTest, Init) {
    // 기본 동작 검증
    std::filesystem::remove("initTest.1");
    std::string filename = "initTest";
    {
        Base::WAL<WalRecordTest, 1000> wal(filename, ApplyEmpty);
        WalRecordTest record = { 1, 2, 3, 4 };
        wal.Write(record);
    }

    {
        std::ifstream fs("initTest.1", std::ios::binary);
        ASSERT_TRUE(fs.is_open());

        uint32_t magic;
        WalRecordTest rec;
        uint32_t crc;

        fs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        fs.read(reinterpret_cast<char*>(&rec), sizeof(rec));
        fs.read(reinterpret_cast<char*>(&crc), sizeof(crc));

        ASSERT_TRUE(fs.good());         // 3번 read 다 성공
        EXPECT_EQ(rec.a, 1);
        EXPECT_EQ(rec.b, 2);
        EXPECT_EQ(rec.c, 3);
        EXPECT_EQ(rec.d, 4);
        EXPECT_EQ(magic, 0xA2DFFD2A);


        char extra;
        fs.read(&extra, 1);
        EXPECT_TRUE(fs.eof());
        EXPECT_EQ(fs.gcount(), 0);
    }
    std::filesystem::remove("initTest.1");
}

TEST(WALTest, Replay) {
    // replay 필드 조회 검증
    std::filesystem::remove("replayTest.1");
    std::string filename = "replayTest";
    {
        Base::WAL<WalRecordTest, 1000> wal(filename, ApplyEmpty);
        for (unsigned long long i = 1; i <= 100; i++) {
            WalRecordTest record1 = { i, i, i, i };
            wal.Write(record1);
        }
    }
    replayCnt = 1;
    {
        Base::WAL<WalRecordTest, 1000> wal(filename, ApplyRecordCompare);
    }
    EXPECT_EQ(replayCnt, 101);

    {
        std::ifstream fs("replayTest.1", std::ios::binary);
        ASSERT_TRUE(fs.is_open());

        uint32_t magic;
        WalRecordTest rec;
        uint32_t crc;
        for (unsigned long long i = 1; i <= 100; i++) {
            fs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            fs.read(reinterpret_cast<char*>(&rec), sizeof(rec));
            fs.read(reinterpret_cast<char*>(&crc), sizeof(crc));

            ASSERT_TRUE(fs.good());         
            EXPECT_EQ(rec.a, i);
            EXPECT_EQ(rec.b, i);
            EXPECT_EQ(rec.c, i);
            EXPECT_EQ(rec.d, i);
            EXPECT_EQ(magic, 0xA2DFFD2A);
        }
    }
    std::filesystem::remove("replayTest.1");
}

TEST(WALTest, Segment) {
    // replay 필드 조회 검증
    std::filesystem::remove("segmentTest.1");
    std::filesystem::remove("segmentTest.2");
    std::filesystem::remove("segmentTest.3");

    std::string filename = "segmentTest";
    {
        Base::WAL<WalRecordTest, 10> wal(filename, ApplyEmpty);
        for (unsigned long long i = 1; i <= 15; i++) {
            WalRecordTest record1 = { i, i, i, i };
            wal.Write(record1);
        }
    }
    {
        Base::WAL<WalRecordTest, 10> wal(filename, ApplyEmpty);
        for (unsigned long long i = 16; i <= 29; i++) {
            WalRecordTest record1 = { i, i, i, i };
            wal.Write(record1);
        }
    }

    for (int i = 1; i <= 3; i++)
    {
        std::ifstream fs("segmentTest."+ std::to_string(i), std::ios::binary);
        ASSERT_TRUE(fs.is_open());

        uint32_t magic;
        WalRecordTest rec;
        uint32_t crc;
        for (unsigned long long j = 1; j <= 10; j++) {
            fs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            fs.read(reinterpret_cast<char*>(&rec), sizeof(rec));
            fs.read(reinterpret_cast<char*>(&crc), sizeof(crc));

            if (j == 10 && i == 3) {
                ASSERT_FALSE(fs.good());
                continue;
            }
            ASSERT_TRUE(fs.good());
            EXPECT_EQ(rec.a, (i - 1) * 10 + j);
            EXPECT_EQ(rec.b, (i - 1) * 10 + j);
            EXPECT_EQ(rec.c, (i - 1) * 10 + j);
            EXPECT_EQ(rec.d, (i - 1) * 10 + j);
            EXPECT_EQ(magic, 0xA2DFFD2A);
        }
    }
    std::filesystem::remove("segmentTest.1");
    std::filesystem::remove("segmentTest.2");
    std::filesystem::remove("segmentTest.3");
}

TEST(WALTest, CorruptedTailRecovery) {
    std::filesystem::remove("corruptTest.1");
    std::string filename = "corruptTest";
    {
        Base::WAL<WalRecordTest, 1000> wal(filename, ApplyEmpty);
        for (uint64_t i = 1; i <= 5; i++) {
            WalRecordTest r = { i, i, i, i };
            wal.Write(r);
        }
    }
    // 꼬리에 깨진 바이트 append
    {
        std::ofstream fs("corruptTest.1", std::ios::binary | std::ios::app);
        char garbage[7] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
        fs.write(garbage, sizeof(garbage));
    }
    // 재오픈 → 유효 5개만 재생, 깨진 꼬리는 스킵돼야 함
    replayCnt = 1;
    {
        Base::WAL<WalRecordTest, 1000> wal(filename, ApplyRecordCompare);
    }
    EXPECT_EQ(replayCnt, 6u);   // 5개만 재생 (깨진 tail 무시)
    std::filesystem::remove("corruptTest.1");
}