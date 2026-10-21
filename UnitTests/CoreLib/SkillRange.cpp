#include <gtest/gtest.h>

#include <CoreLib/SkillData.h>
#include <array>
#include <cstdint>
#include <utility>

TEST(SkillRange, RectangleUsesMovementDirectionsAndInclusiveBounds)
{
    using Data::SkillRange;
    using Data::SkillRangeType;
    const SkillRange line{ SkillRangeType::Rectangle, 12.0f, 3.0f };
    const std::array<std::pair<uint8_t, std::pair<float, float>>, 4> directions{{
        { 0, { 0.0f, 1.0f } },
        { 1, { 0.0f, -1.0f } },
        { 2, { -1.0f, 0.0f } },
        { 3, { 1.0f, 0.0f } },
    }};

    for (const auto& [dir, forward] : directions)
    {
        const float sideX = dir < 2 ? 1.5f : 0.0f;
        const float sideY = dir < 2 ? 0.0f : 1.5f;
        const float endX = forward.first * 12.0f;
        const float endY = forward.second * 12.0f;
        EXPECT_TRUE(line.InRange(dir, 0, 0, forward.first * 6.0f, forward.second * 6.0f));
        EXPECT_TRUE(line.InRange(dir, 0, 0, endX, endY));
        EXPECT_TRUE(line.InRange(dir, 0, 0, forward.first * 6.0f + sideX, forward.second * 6.0f + sideY));
        EXPECT_FALSE(line.InRange(dir, 0, 0, -forward.first, -forward.second));
        EXPECT_FALSE(line.InRange(dir, 0, 0, forward.first + (dir < 2 ? 1.51f : 0.0f),
            forward.second + (dir >= 2 ? 1.51f : 0.0f)));
        EXPECT_FALSE(line.InRange(dir, 0, 0, forward.first * 12.01f, forward.second * 12.01f));
    }
}
