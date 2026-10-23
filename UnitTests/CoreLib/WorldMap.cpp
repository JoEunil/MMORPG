#include <gtest/gtest.h>
#include <CoreLib/ZoneState.h>
#include <CoreLib/WorldMap.h>
#include <limits>

namespace Core {
    // Reuse the existing narrow ZoneState test/benchmark friendship; no production test hook.
    class ZoneBenchmarkAccess {
    public:
        static uint32_t Admit(ZoneState& zone, uint64_t session, const CharacterState& character) {
            return zone.ImmigrateCharLocked(session, character);
        }
        static CharacterState& Character(ZoneState& zone) { return zone.m_chars.front(); }
        static size_t CheatCount(const ZoneState& zone) { return zone.m_cheatList.size(); }
    };

    TEST(WorldMapTest, ExportedLayoutDefinesZoneBoundsAndAoiCellLookup) {
        for (const auto& layout : WorldMap::Zones) {
            ZoneArea area{};
            ZoneInit(area, static_cast<uint16_t>(layout.id));
            EXPECT_FLOAT_EQ(area.x_min, layout.originX + layout.localBounds.x);
            EXPECT_FLOAT_EQ(area.x_max, layout.originX + layout.localBounds.x + layout.localBounds.width);
            EXPECT_FLOAT_EQ(area.y_min, layout.originY + layout.localBounds.y);
            EXPECT_FLOAT_EQ(area.y_max, layout.originY + layout.localBounds.y + layout.localBounds.height);

            std::array<std::array<Cell, CELLS_X>, CELLS_Y> cells;
            CellInit(cells, area);
            for (int y = 0; y < CELLS_Y; ++y) {
                for (int x = 0; x < CELLS_X; ++x) {
                    const auto& exported = layout.aoiCells[y * CELLS_X + x];
                    const auto& actual = cells[y][x].area;
                    EXPECT_FLOAT_EQ(actual.x_min, layout.originX + exported.x);
                    EXPECT_FLOAT_EQ(actual.x_max, layout.originX + exported.x + exported.width);
                    EXPECT_FLOAT_EQ(actual.y_min, layout.originY + exported.y);
                    EXPECT_FLOAT_EQ(actual.y_max, layout.originY + exported.y + exported.height);
                    const auto [cellX, cellY] = GetCell((actual.x_min + actual.x_max) * .5f,
                        (actual.y_min + actual.y_max) * .5f, area);
                    EXPECT_EQ(cellX, x);
                    EXPECT_EQ(cellY, y);
                }
            }
        }
    }

    TEST(WorldMapTest, RejectsBoundsSweptWallsAndCornerCutsInBothDirections) {
        auto bits = WorldMap::WalkableBits;
        bits.fill(~uint64_t{0});
        const auto block = [&bits](int worldX, int worldY) {
            const int x = static_cast<int>((worldX - WorldMap::OriginX) / WorldMap::TileSize);
            const int y = static_cast<int>((worldY - WorldMap::OriginY) / WorldMap::TileSize);
            const int index = y * WorldMap::Width + x;
            bits[index / 64] &= ~(uint64_t{1} << (index % 64));
        };
        const auto traverse = [&bits](float x1, float y1, float x2, float y2) {
            return WorldMap::Detail::CanTraverse(x1, y1, x2, y2, bits);
        };
        EXPECT_TRUE(traverse(-4.5f, -4.5f, -3.5f, -3.5f));
        EXPECT_TRUE(traverse(WorldMap::OriginX, WorldMap::OriginY, WorldMap::OriginX, WorldMap::OriginY));
        EXPECT_FALSE(traverse(-5.01f, 0, 0, 0));
        EXPECT_FALSE(traverse(0, 0, WorldMap::OriginX + WorldMap::Width * WorldMap::TileSize, 0));
        EXPECT_FALSE(traverse(0, 0, 0, WorldMap::OriginY + WorldMap::Height * WorldMap::TileSize));
        EXPECT_FALSE(traverse(0, 0, (std::numeric_limits<float>::infinity)(), 0));
        EXPECT_FALSE(traverse((std::numeric_limits<float>::quiet_NaN)(), 0, 0, 0));

        block(11, 10);
        EXPECT_FALSE(traverse(10.5f, 10.5f, 12.5f, 10.5f)); // Clear endpoints, wall in between.
        EXPECT_FALSE(traverse(12.5f, 10.5f, 10.5f, 10.5f));
        EXPECT_FALSE(traverse(10.5f, 10.5f, 11.5f, 11.5f)); // Orthogonal wall at a corner.
        EXPECT_FALSE(traverse(11.5f, 11.5f, 10.5f, 10.5f));
        EXPECT_FALSE(traverse(11.0f, 9.5f, 11.0f, 11.5f)); // On a wall edge, either direction.
        EXPECT_FALSE(traverse(11.0f, 11.5f, 11.0f, 9.5f));
        EXPECT_FALSE(traverse(10.5f, 9.5f, 11.0f, 10.0f)); // Closed endpoint touches wall.
        EXPECT_TRUE(traverse(10.5f, 12.5f, 11.0f, 12.0f)); // Mixed-sign exact endpoint corner.
        EXPECT_TRUE(traverse(11.0f, 12.0f, 10.5f, 12.5f));
        block(10, 11);
        EXPECT_FALSE(traverse(10.5f, 10.5f, 11.5f, 11.5f)); // Two touching walls cannot be crossed.
    }

    TEST(WorldMapTest, PlayerChecksBudgetClampedSegmentAndMonsterWanderChecksWorldEdge) {
        ZoneState zone{6};
        const float maxX = WorldMap::OriginX + WorldMap::Width * WorldMap::TileSize;
        const float maxY = WorldMap::OriginY + WorldMap::Height * WorldMap::TileSize;
        const float testY = maxY - 1.5f * WorldMap::TileSize;
        float legalCenterX = maxX - 0.5f * WorldMap::TileSize;
        // Works for both the provisional open envelope and an exported blocked boundary row/column.
        while (legalCenterX > maxX - CELL_SIZE &&
            !WorldMap::CanTraverse(legalCenterX, testY, legalCenterX, testY))
            legalCenterX -= WorldMap::TileSize;
        ASSERT_GT(legalCenterX, maxX - CELL_SIZE);
        const float targetX = legalCenterX + WorldMap::TileSize;
        CharacterState incoming{};
        incoming.x = legalCenterX;
        incoming.y = testY;
        incoming.hp = 100;
        incoming.moveBudget = 0.1f;
        ASSERT_NE(ZoneBenchmarkAccess::Admit(zone, 42, incoming), INVALID_ZONE_INTERNAL_ID);
        zone.Move(42, 3, targetX, incoming.y);
        auto& character = ZoneBenchmarkAccess::Character(zone);
        EXPECT_NEAR(character.x, incoming.x + 0.1f + MOVE_BUDGET_EPSILON, 0.0001f);
        EXPECT_FLOAT_EQ(character.y, incoming.y);
        const float beforeBlockedX = character.x;
        character.moveBudget = MOVE_BUDGET_CAP;
        zone.Move(42, 3, targetX, incoming.y);
        EXPECT_FLOAT_EQ(character.x, beforeBlockedX);
        EXPECT_FLOAT_EQ(character.moveBudget, MOVE_BUDGET_CAP);
        EXPECT_EQ(ZoneBenchmarkAccess::CheatCount(zone), 0u);

        MonsterState monster{};
        monster.Initialize(0, 0, legalCenterX + 0.5f * WorldMap::TileSize -
            0.375f * WORLD_MOVEMENT_SCALE, testY, 4, 4);
        monster.dir = 3;
        monster.wanderTargetX = monster.x + 0.375f * WORLD_MOVEMENT_SCALE;
        const float beforeWanderX = monster.x;
        zone.MoveAround(monster);
        EXPECT_FLOAT_EQ(monster.x, beforeWanderX);
        EXPECT_GT(monster.wanderPauseTicks, 0);
        EXPECT_EQ(monster.dirtyBit, 0);
    }
}
