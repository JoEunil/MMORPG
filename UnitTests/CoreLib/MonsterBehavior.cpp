#include <gtest/gtest.h>

#include <CoreLib/ZoneState.h>
#include <algorithm>
#include <cmath>
#include <set>

namespace Core {
    class MonsterBehaviorFixture {
    public:
        ZoneState zone{1};

        void AddMonster(const MonsterState& monster) { zone.m_monsters.push_back(monster); }

        void AddCharacter(const CharacterState& character) {
            const auto index = static_cast<uint16_t>(zone.m_chars.size());
            zone.m_InternalIDToIndex.emplace(character.zoneInternalID, index);
            zone.m_chars.push_back(character);
        }

        MonsterState& MonsterAt(size_t index) { return zone.m_monsters[index]; }
        size_t ActiveSkillCount() const { return zone.m_cells[2][2].activeSkills.size(); }
        void Update() { zone.UpdateMonster(); }
    };

    TEST(MonsterBehaviorTest, WandersWithinHomeAtScaledSpeedWithIdleVarianceAndRespawnReset) {
        MonsterBehaviorFixture fixture;
        constexpr size_t count = 16;
        constexpr float speedPerTick = 0.6f * WORLD_MOVEMENT_SCALE;
        for (uint16_t id = 0; id < count; ++id) {
            MonsterState monster{};
            monster.Initialize(0, id, 50.0f, 50.0f, 2, 2);
            fixture.AddMonster(monster);
        }
        const uint32_t firstMonsterSeed = fixture.MonsterAt(0).wanderSeed;

        uint32_t idleTicks = 0;
        std::set<std::pair<int, int>> finalPositions;
        for (int tick = 0; tick < 100; ++tick) {
            float previousX[count];
            float previousY[count];
            for (size_t index = 0; index < count; ++index) {
                previousX[index] = fixture.MonsterAt(index).x;
                previousY[index] = fixture.MonsterAt(index).y;
            }
            fixture.Update();
            for (size_t index = 0; index < count; ++index) {
                auto& monster = fixture.MonsterAt(index);
                const float distance = std::abs(monster.x - previousX[index]) + std::abs(monster.y - previousY[index]);
                EXPECT_LE(distance, speedPerTick + 0.0001f);
                if (distance == 0.0f) ++idleTicks;
                EXPECT_LE(std::abs(monster.x - monster.spawnX), 6.0f);
                EXPECT_LE(std::abs(monster.y - monster.spawnY), 6.0f);
                EXPECT_GE(monster.x, 39.0f);
                EXPECT_LE(monster.x, 61.0f);
                EXPECT_GE(monster.y, 39.0f);
                EXPECT_LE(monster.y, 61.0f);
            }
        }

        for (size_t index = 0; index < count; ++index) {
            const auto& monster = fixture.MonsterAt(index);
            finalPositions.emplace(static_cast<int>(std::lround(monster.x * 100.0f)),
                static_cast<int>(std::lround(monster.y * 100.0f)));
        }
        EXPECT_GT(idleTicks, count * 4);
        EXPECT_GT(finalPositions.size(), 1u);

        auto& respawn = fixture.MonsterAt(0);
        respawn.wanderSeed ^= 0x12345678u;
        respawn.wanderTicks = 5;
        respawn.wanderPauseTicks = 4;
        respawn.hp = 0;
        respawn.respawnTick = 1;
        fixture.Update();
        EXPECT_EQ(respawn.wanderSeed, firstMonsterSeed);
        EXPECT_EQ(respawn.wanderTicks, 0);
        EXPECT_EQ(respawn.wanderPauseTicks, 0);
        EXPECT_FLOAT_EQ(respawn.x, respawn.spawnX);
        EXPECT_FLOAT_EQ(respawn.y, respawn.spawnY);
        EXPECT_EQ(respawn.cellX, 2);
        EXPECT_EQ(respawn.cellY, 2);
        EXPECT_EQ(respawn.dirtyBit, 0x0F);
    }

    TEST(MonsterBehaviorTest, AggroTicksAdvanceOnceAndClearOnTimeoutOrMissingTarget) {
        MonsterBehaviorFixture fixture;
        Data::MonsterData timeoutData(0, 100, 1, 30, 1.0f,
            {{1.0f, 0, 2, {{0, 100}, {0, 100}}}}, {0});
        timeoutData.aggroTimeOut = 3;

        MonsterState monster{};
        monster.Initialize(0, 42, 50.0f, 50.0f, 2, 2);
        monster.data = &timeoutData;
        monster.hp = 100;
        monster.aggro = 700;
        fixture.AddMonster(monster);

        CharacterState target{};
        target.zoneInternalID = 700;
        target.x = 55.0f;
        target.y = 50.0f;
        fixture.AddCharacter(target);

        fixture.Update();
        EXPECT_EQ(fixture.MonsterAt(0).aggro, 700u);
        EXPECT_EQ(fixture.MonsterAt(0).aggroTick, 1u);
        fixture.Update();
        EXPECT_EQ(fixture.MonsterAt(0).aggroTick, 2u);
        fixture.Update();
        EXPECT_EQ(fixture.MonsterAt(0).aggro, 0u);
        EXPECT_EQ(fixture.MonsterAt(0).aggroTick, 0u);

        Data::MonsterData strongData(1, 100, 1, 30, 1.2f,
            {{1.0f, 0, 2, {{0, 100}, {0, 100}}}}, {0});
        auto& strong = fixture.MonsterAt(0);
        strong.data = &strongData;
        strong.x = 50.0f;
        strong.y = 50.0f;
        strong.aggro = 700;
        fixture.zone.MoveToward(strong, target);
        EXPECT_NEAR(strong.x, 50.3f, 0.0001f);
        strong.x = 60.9f; // Cell 2 ends at 61; do not migrate cell membership.
        target.x = 80.0f;
        fixture.zone.MoveToward(strong, target);
        EXPECT_FLOAT_EQ(strong.x, 60.9f);
        EXPECT_EQ(strong.aggro, 0u);
        EXPECT_EQ(strong.aggroTick, 0u);

        MonsterBehaviorFixture bossFixture;
        Data::MonsterData bossData(2, 100, 1, 30, 0.6f,
            {{1.0f, 0, 2, {{0, 2}, {0, 2}}}}, {0});
        MonsterState boss{};
        boss.Initialize(2, 7, 50.0f, 50.0f, 2, 2);
        boss.data = &bossData;
        boss.hp = 100;
        boss.aggro = 700;
        bossFixture.AddMonster(boss);
        target.x = 55.0f;
        bossFixture.AddCharacter(target);
        bossFixture.Update();
        EXPECT_EQ(bossFixture.ActiveSkillCount(), 0u);
        bossFixture.Update();
        EXPECT_EQ(bossFixture.ActiveSkillCount(), 1u);
        bossFixture.Update();
        EXPECT_EQ(bossFixture.ActiveSkillCount(), 1u);
        bossFixture.Update();
        EXPECT_EQ(bossFixture.ActiveSkillCount(), 2u);
        bossFixture.MonsterAt(0).hp = 0;
        bossFixture.MonsterAt(0).respawnTick = 2;
        const auto deadAggroTicks = bossFixture.MonsterAt(0).aggroTick;
        bossFixture.Update();
        EXPECT_EQ(bossFixture.MonsterAt(0).aggroTick, deadAggroTicks);

        timeoutData.aggroTimeOut = 100;
        fixture.MonsterAt(0).aggro = 999;
        fixture.MonsterAt(0).aggroTick = 7;
        fixture.Update();
        EXPECT_EQ(fixture.MonsterAt(0).aggro, 0u);
        EXPECT_EQ(fixture.MonsterAt(0).aggroTick, 0u);
    }
}
