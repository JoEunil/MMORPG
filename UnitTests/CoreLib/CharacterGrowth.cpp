#include <gtest/gtest.h>

#include <CoreLib/CharacterState.h>

namespace Core {
    TEST(CharacterGrowthTest, RewardCrossesOnlyNewBoundariesAndPreservesMissingHp) {
        CharacterState character{};
        character.level = 1;
        character.exp = 40;
        character.maxHp = 10000;
        character.hp = 6000;
        character.dirtyBit = 0x02; // Preserve unrelated MP changes.

        character.AddExperience(9);
        EXPECT_EQ(character.exp, 49u);
        EXPECT_EQ(character.level, 1u);
        EXPECT_EQ(character.maxHp, 10000);
        EXPECT_EQ(character.hp, 6000);
        EXPECT_EQ(character.dirtyBit, 0x12u);

        character.dirtyBit = 0;
        character.AddExperience(1);
        EXPECT_EQ(character.exp, 50u);
        EXPECT_EQ(character.level, 2u);
        EXPECT_EQ(character.maxHp, 10500);
        EXPECT_EQ(character.hp, 6500);
        EXPECT_EQ(character.dirtyBit, 0x35u);

        character.dirtyBit = 0;
        character.AddExperience(250);
        EXPECT_EQ(character.exp, 300u);
        EXPECT_EQ(character.level, 4u);
        EXPECT_EQ(character.maxHp, 11500);
        EXPECT_EQ(character.hp, 7500);
        EXPECT_EQ(character.dirtyBit, 0x35u);

        character.dirtyBit = 0;
        character.AddExperience(1);
        EXPECT_EQ(character.exp, 301u);
        EXPECT_EQ(character.level, 4u);
        EXPECT_EQ(character.maxHp, 11500);
        EXPECT_EQ(character.hp, 7500);
        EXPECT_EQ(character.dirtyBit, 0x10u);
    }

    TEST(CharacterGrowthTest, CapsGrowthAndArithmeticWithoutRevivingOrDowngradingSavedCharacters) {
        CharacterState character{};
        character.level = 99;
        character.exp = Data::growthLevels[99].requiredTotalExp - 1;
        character.maxHp = INT32_MAX - 100;
        character.hp = 6000;

        character.AddExperience(1);
        EXPECT_EQ(character.level, 100u);
        EXPECT_EQ(character.maxHp, INT32_MAX);
        EXPECT_EQ(character.hp, 6100); // Only the actual capped increase is restored.
        EXPECT_EQ(character.dirtyBit, 0x35u);

        character.dirtyBit = 0;
        character.AddExperience(UINT32_MAX);
        EXPECT_EQ(character.exp, UINT32_MAX);
        EXPECT_EQ(character.level, 100u);
        EXPECT_EQ(character.maxHp, INT32_MAX);
        EXPECT_EQ(character.hp, 6100);
        EXPECT_EQ(character.dirtyBit, 0x10u);
        character.dirtyBit = 0;
        character.AddExperience(1);
        EXPECT_EQ(character.exp, UINT32_MAX);
        EXPECT_EQ(character.dirtyBit, 0u);

        CharacterState dead{};
        dead.level = 1;
        dead.exp = UINT32_MAX; // A saved low level with high EXP waits for a normal reward.
        dead.maxHp = 10000;
        EXPECT_EQ(dead.level, 1u);
        EXPECT_EQ(dead.hp, 0);
        EXPECT_EQ(dead.maxHp, 10000);
        EXPECT_EQ(dead.dirtyBit, 0u);
        dead.AddExperience(10);
        EXPECT_EQ(dead.exp, UINT32_MAX);
        EXPECT_EQ(dead.level, 100u);
        EXPECT_EQ(dead.maxHp, 59500);
        EXPECT_EQ(dead.hp, 0);
        EXPECT_EQ(dead.dirtyBit, 0x24u);

        CharacterState capped{};
        capped.level = 2;
        capped.exp = 149;
        capped.maxHp = INT32_MAX;
        capped.hp = 1;
        capped.AddExperience(1);
        EXPECT_EQ(capped.level, 3u);
        EXPECT_EQ(capped.maxHp, INT32_MAX);
        EXPECT_EQ(capped.hp, 1);
        EXPECT_EQ(capped.dirtyBit, 0x30u);

        CharacterState overflowingHp{};
        overflowingHp.level = 1;
        overflowingHp.exp = 49;
        overflowingHp.maxHp = 10000;
        overflowingHp.hp = INT32_MAX - 1;
        overflowingHp.AddExperience(1);
        EXPECT_EQ(overflowingHp.maxHp, 10500);
        EXPECT_EQ(overflowingHp.hp, INT32_MAX);
        EXPECT_EQ(overflowingHp.dirtyBit, 0x35u);

        CharacterState saved{};
        saved.level = 120;
        saved.exp = UINT32_MAX - 1;
        saved.maxHp = 12345;
        saved.hp = 2345;
        saved.AddExperience(10);
        EXPECT_EQ(saved.exp, UINT32_MAX);
        EXPECT_EQ(saved.level, 120u);
        EXPECT_EQ(saved.maxHp, 12345);
        EXPECT_EQ(saved.hp, 2345);
        EXPECT_EQ(saved.dirtyBit, 0x10u);
    }
}
