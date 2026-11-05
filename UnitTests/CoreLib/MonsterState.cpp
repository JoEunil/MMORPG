#include <gtest/gtest.h>

#include <CoreLib/MonsterState.h>
#include <CoreLib/HuntingZoneData.h>

namespace Core {
    TEST(MonsterStateTest, HuntingDataKeepsCombatShapeAndUsesZoneStatsAndRewards) {
        const uint32_t hp[] = {900, 1200, 1600, 2100, 2700};
        const uint32_t strongHp[] = {2000, 2800, 3800, 5000, 6500};
        const uint32_t attacks[] = {50, 70, 90, 120, 150};
        const uint32_t strongAttacks[] = {80, 110, 150, 190, 240};
        const uint32_t exp[] = {10, 25, 40, 55, 70};
        const uint64_t gold[] = {10, 15, 20, 30, 40};
        EXPECT_THROW(Data::GetHuntingMonsterData(1, 0), std::out_of_range);
        EXPECT_THROW(Data::GetHuntingMonsterData(2, 2), std::out_of_range);
        for (uint16_t zone = 2; zone <= 6; ++zone) {
            SCOPED_TRACE(zone);
            for (uint16_t type = 0; type < 3; ++type) {
                if (type == 2 && zone != 4 && zone != 6) continue;
                const auto& data = Data::GetHuntingMonsterData(zone, type);
                const auto& shape = Data::monsters[type];
                EXPECT_EQ(&data, &Data::GetHuntingMonsterData(zone, type));
                EXPECT_EQ(data.id, shape.id);
                EXPECT_EQ(data.respawn, shape.respawn);
                EXPECT_FLOAT_EQ(data.moveSpeed, shape.moveSpeed);
                EXPECT_EQ(data.aggroTimeOut, shape.aggroTimeOut);
                EXPECT_FLOAT_EQ(data.chaseDistance, shape.chaseDistance);
                ASSERT_EQ(data.phasesCnt, shape.phasesCnt);
                for (size_t phase = 0; phase < data.phasesCnt; ++phase) {
                    EXPECT_FLOAT_EQ(data.phases[phase].hpThreshold, shape.phases[phase].hpThreshold);
                    EXPECT_EQ(data.phases[phase].hpThresholdAbs,
                        static_cast<uint32_t>(data.maxHp * shape.phases[phase].hpThreshold));
                    ASSERT_EQ(data.phases[phase].stepCnt, shape.phases[phase].stepCnt);
                    for (size_t step = 0; step < data.phases[phase].stepCnt; ++step) {
                        EXPECT_EQ(data.phases[phase].steps[step].skillID, shape.phases[phase].steps[step].skillID);
                        EXPECT_EQ(data.phases[phase].steps[step].delayTicks, shape.phases[phase].steps[step].delayTicks);
                    }
                }
                if (type < 2) {
                    EXPECT_EQ(data.maxHp, type == 0 ? hp[zone - 2] : strongHp[zone - 2]);
                    EXPECT_EQ(data.attack, type == 0 ? attacks[zone - 2] : strongAttacks[zone - 2]);
                    EXPECT_EQ(data.reward.exp, exp[zone - 2] * (type == 0 ? 1 : 3));
                    EXPECT_EQ(data.reward.gold, gold[zone - 2] * (type == 0 ? 1 : 3));
                } else {
                    EXPECT_EQ(data.maxHp, zone == 4 ? 24000u : 36000u);
                    EXPECT_EQ(data.attack, zone == 4 ? 18u : 30u);
                    EXPECT_EQ(data.reward.exp, zone == 4 ? 1200u : 2100u);
                    EXPECT_EQ(data.reward.gold, 0u);
                }
                MonsterState monster{};
                monster.Initialize(data, 42, 100.0f, 100.0f, 2, 2);
                EXPECT_EQ(monster.data, &data);
                EXPECT_EQ(monster.hp, data.maxHp);
                monster.hp = 0;
                monster.Respawn();
                EXPECT_EQ(monster.hp, data.maxHp);
            }
        }
        ASSERT_EQ(Data::skillList.size(), Data::monsterSkillList.size());
        const uint32_t playerAttacks[] = {30, 120, 180};
        const uint32_t mana[] = {0, 100, 160};
        const uint32_t cooldown[] = {20, 120, 80};
        for (size_t id = 0; id < Data::skillList.size(); ++id) {
            const auto& player = Data::skillList[id];
            const auto& monster = Data::monsterSkillList[id];
            if (id < 3) {
                EXPECT_EQ(player.phases[1].attack, playerAttacks[id]);
                EXPECT_EQ(player.mana, mana[id]);
                EXPECT_EQ(player.coolDown, cooldown[id]);
            }
            ASSERT_EQ(player.phases.size(), monster.phases.size());
            for (size_t phase = 0; phase < player.phases.size(); ++phase) {
                EXPECT_EQ(player.phases[phase].tick, monster.phases[phase].tick);
                EXPECT_EQ(player.phases[phase].actionID, monster.phases[phase].actionID);
                EXPECT_EQ(player.phases[phase].range.type, monster.phases[phase].range.type);
                EXPECT_FLOAT_EQ(player.phases[phase].range.range1, monster.phases[phase].range.range1);
                EXPECT_FLOAT_EQ(player.phases[phase].range.range2, monster.phases[phase].range.range2);
            }
        }
        EXPECT_EQ(Data::monsterSkillList[0].phases[1].attack, 5u);
        EXPECT_EQ(Data::monsterSkillList[1].phases[1].attack, 15u);
        EXPECT_EQ(Data::monsterSkillList[3].phases[1].attack, 100u);
        EXPECT_EQ(Data::monsterSkillList[4].phases[1].attack, 100u);
    }

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
        EXPECT_FLOAT_EQ(monster.aggroX, monster.spawnX);
        EXPECT_FLOAT_EQ(monster.aggroY, monster.spawnY);
        EXPECT_FLOAT_EQ(monster.wanderTargetX, monster.spawnX);
        EXPECT_FLOAT_EQ(monster.wanderTargetY, monster.spawnY);
    }

    TEST(MonsterStateTest, AggroPositionChangesOnlyWhenStartingANewCombat) {
        MonsterState monster{};
        monster.Initialize(0, 0, 100.0f, 100.0f, 2, 2);
        monster.x = 105.0f;
        monster.SetAggro(700);
        monster.x = 110.0f;
        monster.SetAggro(701);
        EXPECT_FLOAT_EQ(monster.aggroX, 105.0f);
        EXPECT_EQ(monster.aggro, 701u);
        monster.aggro = 0;
        monster.SetAggro(702);
        EXPECT_FLOAT_EQ(monster.aggroX, 110.0f);
    }
}
