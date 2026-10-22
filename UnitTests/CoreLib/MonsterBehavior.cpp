#include <gtest/gtest.h>

#include <CoreLib/ZoneState.h>
#include <CoreLib/WorldMap.h>
#include <algorithm>
#include <cmath>
#include <set>

namespace Core {
    class MonsterBehaviorFixture {
    public:
        explicit MonsterBehaviorFixture(uint16_t zoneID = 1) : zone(zoneID) {}
        ZoneState zone;

        void AddMonster(const MonsterState& monster) { zone.m_monsters.push_back(monster); }

        void AddCharacter(const CharacterState& character) {
            const auto index = static_cast<uint16_t>(zone.m_chars.size());
            zone.m_InternalIDToIndex.emplace(character.zoneInternalID, index);
            zone.m_chars.push_back(character);
        }

        MonsterState& MonsterAt(size_t index) { return zone.m_monsters[index]; }
        size_t ActiveSkillCount() const { return zone.m_cells[2][2].activeSkills.size(); }
        void Update() { zone.UpdateMonster(); }
        void InitializeSpawns() { zone.InitializeMonster(); }
        const std::vector<MonsterState>& Monsters() const { return zone.m_monsters; }
        const Cell& CellAt(int x, int y) const { return zone.m_cells[y][x]; }

    };

    TEST(SpawnLayoutTest, StarterLayoutLeavesEntryCellsEmptyAndBuildsAllServerReferences) {
        for (uint16_t zoneID = 1; zoneID <= ZONE_COUNT; ++zoneID) {
            SCOPED_TRACE(zoneID);
            MonsterBehaviorFixture fixture(zoneID);
            fixture.InitializeSpawns();
            if (zoneID == TOWN_ZONE_ID) {
                ASSERT_TRUE(fixture.Monsters().empty());
                for (int y = 0; y < CELLS_Y; ++y)
                    for (int x = 0; x < CELLS_X; ++x)
                        EXPECT_TRUE(fixture.CellAt(x, y).monsterIndexes.empty());
                continue;
            }
            const int regular[25] = {0,0,2,3,4, 0,2,3,4,5, 2,3,0,5,6, 3,4,5,6,7, 4,5,6,7,8};
            const int strong[25] = {0,0,0,0,1, 0,0,0,1,1, 0,0,0,1,1, 0,1,1,1,2, 1,1,1,2,2};
            ASSERT_EQ(fixture.Monsters().size(), 112u);
            std::set<uint32_t> seen;
            int regularTotal = 0, strongTotal = 0, bossTotal = 0;
            for (int y = 0; y < 5; ++y) {
                for (int x = 0; x < 5; ++x) {
                    const auto& cell = fixture.CellAt(x, y);
                    const bool boss = x == 2 && y == 2;
                    const int slot = y * 5 + x;
                    ASSERT_EQ(cell.monsterIndexes.size(), static_cast<size_t>(boss ? 1 : regular[slot] + strong[slot]));
                    for (size_t n = 0; n < cell.monsterIndexes.size(); ++n) {
                        const auto index = cell.monsterIndexes[n];
                        ASSERT_LT(index, fixture.Monsters().size());
                        EXPECT_TRUE(seen.insert(index).second);
                        const auto& monster = fixture.Monsters()[index];
                        EXPECT_EQ(monster.internalID, index);
                        EXPECT_EQ(monster.cellX, x); EXPECT_EQ(monster.cellY, y);
                        EXPECT_EQ(monster.data->id, boss ? 2 : (n < static_cast<size_t>(regular[slot]) ? 0 : 1));
                        EXPECT_FLOAT_EQ(monster.x, monster.spawnX); EXPECT_FLOAT_EQ(monster.y, monster.spawnY);
                        EXPECT_TRUE(WorldMap::CanTraverse(monster.x, monster.y, monster.x, monster.y));
                        if (boss) {
                            ++bossTotal;
                            EXPECT_FLOAT_EQ(monster.x, (cell.area.x_min + cell.area.x_max) / 2);
                            EXPECT_FLOAT_EQ(monster.y, (cell.area.y_min + cell.area.y_max) / 2);
                        } else {
                            if (monster.data->id == 0) ++regularTotal; else ++strongTotal;
                            EXPECT_GE(monster.x, cell.area.x_min + 1); EXPECT_LE(monster.x, cell.area.x_max - 1);
                            EXPECT_GE(monster.y, cell.area.y_min + 1); EXPECT_LE(monster.y, cell.area.y_max - 1);
                        }
                    }
                }
            }
            EXPECT_EQ(regularTotal, 94); EXPECT_EQ(strongTotal, 17); EXPECT_EQ(bossTotal, 1);
            EXPECT_EQ(seen.size(), 112u);
        }
    }

    TEST(MonsterBehaviorTest, WandersWithinHomeAtScaledSpeedWithIdleVarianceAndRespawnReset) {
        MonsterBehaviorFixture fixture;
        constexpr size_t count = 16;
        constexpr float speedPerTick = 0.075f;
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
        EXPECT_NEAR(strong.x, 50.18f, 0.0001f);
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

        MonsterBehaviorFixture rangeFixture;
        Data::MonsterData regularData(0, 100, 1, 30, 1.0f,
            {{1.0f, 0, 2, {{0, 100}, {0, 100}}}}, {0});
        MonsterState regular{};
        regular.Initialize(0, 42, 50.0f, 50.0f, 2, 2);
        regular.data = &regularData;
        regular.hp = 100;
        regular.aggro = 701;
        regular.tick = 100;
        rangeFixture.AddMonster(regular);
        CharacterState justOutside{};
        justOutside.zoneInternalID = 701;
        justOutside.x = 51.682f;
        justOutside.y = 51.987f;
        rangeFixture.AddCharacter(justOutside);

        rangeFixture.Update();
        EXPECT_EQ(rangeFixture.ActiveSkillCount(), 0u);
        EXPECT_FLOAT_EQ(rangeFixture.MonsterAt(0).x, 50.0f);
        EXPECT_NEAR(rangeFixture.MonsterAt(0).y, 50.15f, 0.0001f);

        CharacterState circleBoundary{};
        circleBoundary.zoneInternalID = 702;
        circleBoundary.x = 52.0f;
        circleBoundary.y = rangeFixture.MonsterAt(0).y;
        rangeFixture.AddCharacter(circleBoundary);
        rangeFixture.MonsterAt(0).aggro = 702;
        rangeFixture.MonsterAt(0).tick = 100;
        rangeFixture.Update();
        ASSERT_EQ(rangeFixture.ActiveSkillCount(), 1u);
        const auto& skillZero = rangeFixture.CellAt(2, 2).activeSkills[0];
        EXPECT_EQ(skillZero.skillId, 0u);
        EXPECT_FLOAT_EQ(skillZero.x, rangeFixture.MonsterAt(0).x);
        EXPECT_FLOAT_EQ(skillZero.y, rangeFixture.MonsterAt(0).y);

        Data::MonsterData skillOneData(1, 100, 1, 30, 1.2f,
            {{1.0f, 0, 2, {{1, 100}, {1, 100}}}}, {1});
        MonsterState strongCaster{};
        strongCaster.Initialize(1, 43, 60.0f, 60.0f, 2, 2);
        strongCaster.data = &skillOneData;
        strongCaster.hp = 100;
        strongCaster.aggro = 703;
        strongCaster.tick = 100;
        rangeFixture.AddMonster(strongCaster);
        CharacterState insideSkillOneRange{};
        insideSkillOneRange.zoneInternalID = 703;
        insideSkillOneRange.x = 61.682f;
        insideSkillOneRange.y = 61.987f;
        rangeFixture.AddCharacter(insideSkillOneRange);
        rangeFixture.Update();
        ASSERT_EQ(rangeFixture.ActiveSkillCount(), 2u);
        const auto& skillOne = rangeFixture.CellAt(2, 2).activeSkills[1];
        EXPECT_EQ(skillOne.skillId, 1u);
        EXPECT_FLOAT_EQ(skillOne.x, 60.0f);
        EXPECT_FLOAT_EQ(skillOne.y, 60.0f);
    }
}
