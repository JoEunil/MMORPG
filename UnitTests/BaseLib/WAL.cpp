#include <gtest/gtest.h>

#include <BaseLib/WAL.h>

#pragma pack(push,1) 
struct WalRecordTestA {
    uint64_t a;
    uint32_t b;
    uint16_t c;
    uint8_t  d;
};
struct WalRecordTestB {
    uint64_t c;
    uint32_t b;
    uint16_t a;
};
#pragma pack(pop)

uint8_t typeA = 1;
uint8_t typeB = 2;

void ApplyEmpty(const Base::WALHeader& h, const uint8_t* p) {
}

int replayCnt = 1;
void ApplyRecordCompare(const Base::WALHeader& h, const uint8_t* p) {
    ASSERT_EQ(h.type, typeA);
    EXPECT_EQ(h.magic, 0xA2DFFD2A);
    const WalRecordTestA* rec = reinterpret_cast<const WalRecordTestA*>(p);
    EXPECT_EQ(rec->a, replayCnt);
    EXPECT_EQ(rec->b, replayCnt);
    EXPECT_EQ(rec->c, replayCnt);
    EXPECT_EQ(rec->d, replayCnt);
    replayCnt++;
}

TEST(WALTest, Init) {
    // 기본 동작 검증
    std::filesystem::remove("initTest.1");
    std::string filename = "initTest";
    {
        Base::WAL wal(filename, 100, ApplyEmpty);
        WalRecordTestA record = { 1, 2, 3, 4 };
        wal.Write(reinterpret_cast<uint8_t*>(&record), sizeof(WalRecordTestA), typeA);
    }

    {
        std::ifstream fs("initTest.1", std::ios::binary);
        ASSERT_TRUE(fs.is_open());

        Base::WALHeader header;
        WalRecordTestA payload;

        fs.read(reinterpret_cast<char*>(&header), sizeof(header));
        fs.read(reinterpret_cast<char*>(&payload), header.length);
        ASSERT_TRUE(fs.good());
        ASSERT_EQ(header.length, sizeof(WalRecordTestA));
        EXPECT_EQ(header.magic, 0xA2DFFD2A);
        EXPECT_EQ(header.type, typeA);
        EXPECT_NE(header.crc, 0);

        EXPECT_EQ(payload.a, 1);
        EXPECT_EQ(payload.b, 2);
        EXPECT_EQ(payload.c, 3);
        EXPECT_EQ(payload.d, 4);


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
        Base::WAL wal(filename, 3500, ApplyEmpty);
        for (unsigned long long i = 1; i <= 100; i++) {
            // record 1개 15byte + header 20byte = 35byte
            WalRecordTestA record1 = { i, i, i, i };
            wal.Write(reinterpret_cast<uint8_t*>(&record1), sizeof(WalRecordTestA), typeA);
        }
    }
    replayCnt = 1;
    {
        Base::WAL wal(filename, 3500, ApplyRecordCompare);
    }
    EXPECT_EQ(replayCnt, 101);

    {
        std::ifstream fs("replayTest.1", std::ios::binary);
        ASSERT_TRUE(fs.is_open());

        Base::WALHeader header;
        WalRecordTestA payload;

        for (unsigned long long i = 1; i <= 100; i++) {
            fs.read(reinterpret_cast<char*>(&header), sizeof(header));
            fs.read(reinterpret_cast<char*>(&payload), sizeof(payload));

            ASSERT_TRUE(fs.good());
            ASSERT_EQ(header.length, sizeof(WalRecordTestA));
            EXPECT_EQ(header.magic, 0xA2DFFD2A);
            EXPECT_EQ(header.type, typeA);
            EXPECT_NE(header.crc, 0);

            EXPECT_EQ(payload.a, i);
            EXPECT_EQ(payload.b, i);
            EXPECT_EQ(payload.c, i);
            EXPECT_EQ(payload.d, i);
        }
    }
    std::filesystem::remove("replayTest.1");
}

TEST(WALTest, Segment)
{
    std::filesystem::remove("segmentTest.1");
    std::filesystem::remove("segmentTest.2");
    std::filesystem::remove("segmentTest.3");

    std::string filename = "segmentTest";

    {
        Base::WAL wal(filename, 350, ApplyEmpty);

        for (uint64_t i = 1; i <= 15; ++i)
        {
            WalRecordTestA rec = { i,i,(uint16_t)i,(uint8_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&rec),
                sizeof(rec),
                typeA);
        }
    }

    {
        Base::WAL wal(filename, 350, ApplyEmpty);

        for (uint64_t i = 16; i <= 29; ++i)
        {
            WalRecordTestA rec = { i,i,(uint16_t)i,(uint8_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&rec),
                sizeof(rec),
                typeA);
        }
    }

    for (int seg = 1; seg <= 3; ++seg)
    {
        std::ifstream fs("segmentTest." + std::to_string(seg),
            std::ios::binary);

        ASSERT_TRUE(fs.is_open());

        for (int j = 1; j <= 10; ++j)
        {
            Base::WALHeader header;
            WalRecordTestA payload;

            fs.read(reinterpret_cast<char*>(&header), sizeof(header));

            if (seg == 3 && j == 10)
            {
                EXPECT_FALSE(fs.good());
                break;
            }

            fs.read(reinterpret_cast<char*>(&payload), header.length);

            ASSERT_TRUE(fs.good());

            EXPECT_EQ(header.magic, 0xA2DFFD2A);
            EXPECT_EQ(header.type, typeA);
            EXPECT_EQ(header.length, sizeof(WalRecordTestA));

            uint64_t expected = (seg - 1) * 10 + j;

            EXPECT_EQ(payload.a, expected);
            EXPECT_EQ(payload.b, expected);
            EXPECT_EQ(payload.c, expected);
            EXPECT_EQ(payload.d, expected);
        }
    }

    std::filesystem::remove("segmentTest.1");
    std::filesystem::remove("segmentTest.2");
    std::filesystem::remove("segmentTest.3");
}

TEST(WALTest, CorruptedCrcRecovery)
{
    // 헤더는 온전하고 payload만 깨진 레코드 → CRC 검증 경로에서 중단 + 절단
    std::filesystem::remove("crcTest.1");

    std::string filename = "crcTest";
    constexpr size_t RECORD_SIZE = sizeof(Base::WALHeader) + sizeof(WalRecordTestA); // 35

    {
        Base::WAL wal(filename, 1000, ApplyEmpty);

        for (uint64_t i = 1; i <= 5; ++i)
        {
            WalRecordTestA rec = { i,i,(uint16_t)i,(uint8_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&rec),
                sizeof(rec),
                typeA);
        }
    }

    {
        // 5번째 레코드의 payload 내부 1바이트 손상
        std::fstream fs("crcTest.1",
            std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(fs.is_open());

        fs.seekp(4 * RECORD_SIZE + sizeof(Base::WALHeader) + 3);
        char corrupt = 0x5A;
        fs.write(&corrupt, 1);
    }

    replayCnt = 1;

    {
        Base::WAL wal(filename, 1000, ApplyRecordCompare);
    }

    EXPECT_EQ(replayCnt, 5); // 1~4만 유효
    // 손상 레코드부터 절단됐는지 확인
    EXPECT_EQ(std::filesystem::file_size("crcTest.1"), 4 * RECORD_SIZE);

    std::filesystem::remove("crcTest.1");
}

TEST(WALTest, TruncateAndAppend)
{
    // torn tail 절단 후 이어쓰기 -> 재시작 시 빈틈 없는 replay 확인
    std::filesystem::remove("appendTest.1");

    std::string filename = "appendTest";
    constexpr size_t RECORD_SIZE = sizeof(Base::WALHeader) + sizeof(WalRecordTestA); // 35

    {
        Base::WAL wal(filename, 1000, ApplyEmpty);

        for (uint64_t i = 1; i <= 5; ++i)
        {
            WalRecordTestA rec = { i,i,(uint16_t)i,(uint8_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&rec),
                sizeof(rec),
                typeA);
        }
    }

    {
        // 쓰다 만 꼬리 흉내 (헤더 크기 미만)
        std::ofstream fs("appendTest.1", std::ios::binary | std::ios::app);

        char garbage[7] =
        {
            0x11,0x22,0x33,0x44,0x55,0x66,0x77
        };

        fs.write(garbage, sizeof(garbage));
    }

    replayCnt = 1;

    {
        Base::WAL wal(filename, 1000, ApplyRecordCompare);
        EXPECT_EQ(replayCnt, 6); // 5개 replay, 꼬리는 무시

        // 절단된 자리에 정확히 이어쓰기
        WalRecordTestA rec = { 6, 6, 6, 6 };
        wal.Write(reinterpret_cast<uint8_t*>(&rec),
            sizeof(rec),
            typeA);
    }

    // 쓰레기 7바이트가 사라지고 6번째 레코드가 그 자리에 붙었는지
    EXPECT_EQ(std::filesystem::file_size("appendTest.1"), 6 * RECORD_SIZE);

    replayCnt = 1;

    {
        Base::WAL wal(filename, 1000, ApplyRecordCompare);
    }

    EXPECT_EQ(replayCnt, 7); // 절단 지점 이후 레코드까지 전부 복구

    std::filesystem::remove("appendTest.1");
}
int replayCntA = 1;
int replayCntB = 1;
void ApplyMixedCompare(const Base::WALHeader& h, const uint8_t* p) {
    if (h.type == typeA) {
        ASSERT_EQ(h.length, sizeof(WalRecordTestA));
        const WalRecordTestA* rec = reinterpret_cast<const WalRecordTestA*>(p);
        EXPECT_EQ(rec->a, replayCntA);
        EXPECT_EQ(rec->b, replayCntA);
        EXPECT_EQ(rec->c, replayCntA);
        EXPECT_EQ(rec->d, replayCntA);
        replayCntA++;
    }
    else if (h.type == typeB) {
        ASSERT_EQ(h.length, sizeof(WalRecordTestB));
        const WalRecordTestB* rec = reinterpret_cast<const WalRecordTestB*>(p);
        EXPECT_EQ(rec->c, replayCntB);
        EXPECT_EQ(rec->b, replayCntB);
        EXPECT_EQ(rec->a, replayCntB);
        replayCntB++;
    }
    else {
        FAIL() << "unknown wal type: " << h.type;
    }
}

TEST(WALTest, MultiTypeDispatch)
{
    // 타입 A/B 교차 기록 -> replay에서 type별 분기·길이 검증
    std::filesystem::remove("multiTypeTest.1");

    std::string filename = "multiTypeTest";

    {
        Base::WAL wal(filename, 2000, ApplyEmpty);

        for (uint64_t i = 1; i <= 10; ++i)
        {
            WalRecordTestA recA = { i,i,(uint16_t)i,(uint8_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&recA),
                sizeof(recA),
                typeA);

            WalRecordTestB recB = { i,(uint32_t)i,(uint16_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&recB),
                sizeof(recB),
                typeB);
        }
        // (35 + 34) * 10 = 690byte → 단일 세그먼트
    }

    replayCntA = 1;
    replayCntB = 1;

    {
        Base::WAL wal(filename, 2000, ApplyMixedCompare);
    }

    EXPECT_EQ(replayCntA, 11);
    EXPECT_EQ(replayCntB, 11);

    std::filesystem::remove("multiTypeTest.1");
}

TEST(WALTest, TruncateBefore)
{
    std::filesystem::remove("truncateTest.1");
    std::filesystem::remove("truncateTest.2");
    std::filesystem::remove("truncateTest.3");

    std::string filename = "truncateTest";

    {
        Base::WAL wal(filename, 350, ApplyEmpty);

        // 35byte × 10 = 350, seg3(활성): 21~25
        for (uint64_t i = 1; i <= 25; ++i)
        {
            WalRecordTestA rec = { i,i,(uint16_t)i,(uint8_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&rec),
                sizeof(rec),
                typeA);
        }

        // seg1, seg2 삭제 
        wal.TruncateBefore(3);

        EXPECT_FALSE(std::filesystem::exists("truncateTest.1"));
        EXPECT_FALSE(std::filesystem::exists("truncateTest.2"));
        EXPECT_TRUE(std::filesystem::exists("truncateTest.3"));

        // 경계가 활성 세그먼트를 넘어도 활성은 절대 삭제되지 않음 (clamp)
        wal.TruncateBefore(UINT32_MAX);
        EXPECT_TRUE(std::filesystem::exists("truncateTest.3"));

        // truncate 후에도 활성 세그먼트에 이어쓰기 정상
        WalRecordTestA rec = { 26, 26, 26, 26 };
        wal.Write(reinterpret_cast<uint8_t*>(&rec),
            sizeof(rec),
            typeA);
    }

    replayCnt = 21;

    {
        Base::WAL wal(filename, 350, ApplyRecordCompare);
    }

    EXPECT_EQ(replayCnt, 27);

    std::filesystem::remove("truncateTest.3");
}

std::vector<uint64_t> g_replayed;
void ApplyCollect(const Base::WALHeader& h, const uint8_t* p) {
    const WalRecordTestA* rec = reinterpret_cast<const WalRecordTestA*>(p);
    g_replayed.push_back(rec->a);
}

TEST(WALTest, QuarantineCorruptedClosedSegment)
{
    // 이미 닫힌(fsync된) 세그먼트가 깨진 경우:
    //  - 뒤 세그먼트는 계속 replay (full image라 중단이 오히려 손해)
    //  - 손상 세그먼트는 네임스페이스 밖으로 격리
    //  - 그 자리에 이어쓰지 않고 새 번호에서 재개 (LSN 역행 방지)
    const std::string filename = "quarantineTest";
    constexpr size_t RECORD_SIZE = sizeof(Base::WALHeader) + sizeof(WalRecordTestA); // 35

    auto cleanup = [&] {
        for (auto& e : std::filesystem::directory_iterator(".")) {
            auto name = e.path().filename().string();
            if (name.rfind(filename + ".", 0) == 0) {
                std::error_code ec;
                std::filesystem::remove(e.path(), ec);
            }
        }
    };
    cleanup();

    {
        Base::WAL wal(filename, 350, ApplyEmpty); // 35 * 10 = 350 → 세그먼트당 10개
        for (uint64_t i = 1; i <= 25; ++i) {
            WalRecordTestA rec = { i,i,(uint16_t)i,(uint8_t)i };
            wal.Write(reinterpret_cast<uint8_t*>(&rec), sizeof(rec), typeA);
        }
    }
    ASSERT_TRUE(std::filesystem::exists(filename + ".1"));
    ASSERT_TRUE(std::filesystem::exists(filename + ".2"));
    ASSERT_TRUE(std::filesystem::exists(filename + ".3"));

    {
        // seg2의 5번째 레코드(전체 15번) payload 손상 — 활성(seg3)이 아닌 닫힌 세그먼트
        std::fstream fs(filename + ".2", std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(fs.is_open());
        fs.seekp(4 * RECORD_SIZE + sizeof(Base::WALHeader) + 3);
        char corrupt = 0x5A;
        fs.write(&corrupt, 1);
    }

    g_replayed.clear();
    uint64_t newLsn = 0;
    {
        Base::WAL wal(filename, 350, ApplyCollect);
        EXPECT_TRUE(wal.ReplayDegraded());

        WalRecordTestA rec = { 26, 26, 26, 26 };
        newLsn = wal.Write(reinterpret_cast<uint8_t*>(&rec), sizeof(rec), typeA);
    }

    // seg1 전체(1~10) + seg2 유효 prefix(11~14) + seg3 전체(21~25)
    ASSERT_EQ(g_replayed.size(), 19u);
    EXPECT_EQ(g_replayed.front(), 1u);
    EXPECT_EQ(g_replayed[13], 14u);
    EXPECT_EQ(g_replayed[14], 21u); // 손상 세그먼트 뒤가 버려지지 않았다
    EXPECT_EQ(g_replayed.back(), 25u);

    // 손상 세그먼트 격리 — 재replay·truncate 대상에서 빠지고 분석용으로 보존
    EXPECT_FALSE(std::filesystem::exists(filename + ".2"));
    bool quarantined = false;
    for (auto& e : std::filesystem::directory_iterator(".")) {
        if (e.path().filename().string().rfind(filename + ".2.corrupt.", 0) == 0)
            quarantined = true;
    }
    EXPECT_TRUE(quarantined);

    // 번호 재사용 금지 — seg3 다음인 seg4에서 재개
    EXPECT_TRUE(std::filesystem::exists(filename + ".4"));
    EXPECT_EQ(newLsn >> 32, 4u);
    // 새 LSN은 기존 최대 세그먼트의 어떤 LSN보다 크다 (복구 판정 lsn > dbLsn 유지)
    EXPECT_GT(newLsn, (static_cast<uint64_t>(3) << 32) | 349);

    cleanup();
}