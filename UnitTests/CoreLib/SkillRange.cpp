#include <gtest/gtest.h>

#include <CoreLib/SkillData.h>
#include <array>
#include <cstdint>
#include <utility>
#include <cmath>
#include <limits>

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

TEST(SkillRange, FreeAimRectangleRotatesAndRejectsInvalidAngles)
{
    const Data::SkillRange line{ Data::SkillRangeType::Rectangle, 12.0f, 3.0f };
    for (const float angle : { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f }) {
        const float radians = angle * 3.14159265358979323846f / 180.0f;
        const float fx = std::sin(radians), fy = std::cos(radians);
        EXPECT_TRUE(line.InRangeAngle(angle, 100, 100, 100 + fx * 6, 100 + fy * 6));
        EXPECT_FALSE(line.InRangeAngle(angle, 100, 100, 100 - fx * 2, 100 - fy * 2));
        EXPECT_FALSE(line.InRangeAngle(angle, 100, 100, 100 + fx * 13, 100 + fy * 13));
        EXPECT_FALSE(line.InRangeAngle(angle, 100, 100, 100 + fx * 6 + fy * 2, 100 + fy * 6 - fx * 2));
    }
    for (const float invalid : { -1.0f, 360.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
        EXPECT_FALSE(line.InRangeAngle(invalid, 0, 0, 0, 1));
    // The monster's legacy directional predicate is unchanged.
    EXPECT_TRUE(line.InRange(0, 0, 0, 0, 12));
    EXPECT_FALSE(line.InRange(0, 0, 0, 12, 0));
}
