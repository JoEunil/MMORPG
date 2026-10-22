#include <gtest/gtest.h>

#include <CoreLib/MonsterState.h>

namespace Core {
    TEST(MonsterStateTest, RespawnResetsCombatProgressAndRestoresSpawnState) {
        const Data::MonsterData fixtureData(
            99,
            240,
            12,
            73,
            1.0f,
            {
                {1.0f, 0, 2, {{0, 10}, {1, 10}}},
                {0.5f, 0, 2, {{1, 20}, {0, 20}}},
            },
            {5});

        MonsterState monster{};
        monster.Initialize(0, 42, 11.5f, -8.25f, 6, 9);
        monster.data = &fixtureData;
        monster.hp = 0;
        monster.x = 100.0f;
        monster.y = 200.0f;
        monster.dir = 7;
        monster.aggro = 1234;
        monster.aggroTick = 55;
        monster.tick = 9876;
        monster.phase = 1;
        monster.behavior = MonsterBehavior::Dead;
        monster.skillStep = 3;
        monster.respawnTick = 1;

        monster.Respawn();

        EXPECT_EQ(monster.hp, 240);
        EXPECT_FLOAT_EQ(monster.x, 11.5f);
        EXPECT_FLOAT_EQ(monster.y, -8.25f);
        EXPECT_EQ(monster.dir, 1);
        EXPECT_EQ(monster.aggro, 0u);
        EXPECT_EQ(monster.aggroTick, 0u);
        EXPECT_EQ(monster.respawnTick, 73u);
        EXPECT_EQ(monster.tick, 0u);
        EXPECT_EQ(monster.phase, 0u);
        EXPECT_EQ(monster.behavior, MonsterBehavior::Wandering);
        EXPECT_EQ(monster.skillStep, 0u);
        EXPECT_EQ(monster.internalID, 42);
        EXPECT_EQ(monster.cellX, 6);
        EXPECT_EQ(monster.cellY, 9);
    }
}
