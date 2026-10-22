#pragma once
#include <array>
#include <cstdint>
#include <algorithm>

#include "Config.h"

namespace Core {
    inline constexpr float ZONE_SIZE = WorldMap::ZoneCoreSize;
    inline constexpr int ZONE_HORIZON = WorldMap::ZoneColumns;
    inline constexpr int ZONE_VERTICAL = WorldMap::ZoneRows;
    inline constexpr float TRANSITION_BUFFER = WorldMap::TransitionMargin;

    static constexpr float CELL_SIZE = WorldMap::Zones[0].aoiCellSize;
    static_assert(CELL_SIZE > 0 && ZONE_SIZE > 0 && TRANSITION_BUFFER >= 0);
    static constexpr float ZONE_SPAN = WorldMap::Zones[0].span;
    // Format 1 always exports a uniform 5x5 AOI partition, separate from navigation tiles.
    static constexpr int CELLS_X = 5;
    static constexpr int CELLS_Y = 5;
    static_assert(WorldMap::Zones[0].aoiCells.size() == CELLS_X * CELLS_Y,
        "The exported AOI partition must retain the format's 25 cells.");
    static constexpr int CELL_CAPACITY = MAX_ZONE_CAPACITY / (CELLS_X * CELLS_Y) * 2;
    static_assert(CELLS_X > 0 && CELLS_X <= 256 && CELLS_Y <= 256 && CELL_CAPACITY > 0,
        "AOI Cell dimensions must fit actor cell indices and retain a positive capacity.");

    struct ActionResult {
        uint8_t casterType;
        uint32_t zoneInternalId;
        uint16_t monsterId;
        uint8_t skillSlot;
        uint8_t dir;
        float x, y;
        uint32_t skilIId;
        uint16_t skillPhase;
    };

    struct ActiveSkill {
        uint8_t casterType;
        uint64_t sessionId; // ActionResult 송신용 
        uint32_t zoneInternalId;
        uint16_t monsterId;
        uint8_t skillSlot;
        uint8_t dir;
        float x, y;
        uint32_t skillId;
        uint16_t currentPhase;
        uint32_t currentTick;        // 현재 진행중인 틱
    };

    struct CellArea {
        float x_min, x_max;
        float y_min, y_max;
    };

    struct Cell {
        std::vector<uint64_t> charSessions;
        std::vector<uint16_t> monsterIndexes;
        std::vector<uint32_t> dirtyChar;   // Cell 단위 dirty list
        std::vector< ActionResult> actionResults; // 스킬 이펙트 처리
        std::vector<ActiveSkill> activeSkills; // 시전 중인 스킬 리스트, timeline에 의해 이펙트 처리 요청 및 데미지 적용.
        CellArea area;
    };

    struct ZoneArea {
        float x_min, x_max;
        float y_min, y_max;
    };

    inline std::array<std::vector<int>, CELLS_X* CELLS_Y> AOI;

    inline void InitAOI()
    {
        for (int i = 0; i < CELLS_Y; ++i)
        {
            for (int j = 0; j < CELLS_X; ++j)
            {
                int idx = i * CELLS_X + j;
                AOI[idx].clear();

                // 3x3 AOI → 인접 Cell만 적용
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        int nx = j + dx;
                        int ny = i + dy;

                        // grid 범위 체크
                        if (nx < 0 || nx >= CELLS_X) continue;
                        if (ny < 0 || ny >= CELLS_Y) continue;

                        int nIndex = ny * CELLS_X + nx;
                        AOI[idx].push_back(nIndex);
                    }
                }
            }
        }
    }

    inline void ZoneInit(ZoneArea& z, uint16_t zone) {
        const auto& layout = WorldMap::Zones[zone - 1];
        z.x_min = layout.originX + layout.localBounds.x;
        z.x_max = z.x_min + layout.localBounds.width;
        z.y_min = layout.originY + layout.localBounds.y;
        z.y_max = z.y_min + layout.localBounds.height;
    }

    inline void CellInit(std::array<std::array<Cell, CELLS_X>, CELLS_Y>& cells, const ZoneArea& area) {
        for (int cy = 0; cy < CELLS_Y; ++cy) {
            for (int cx = 0; cx < CELLS_X; ++cx) {

                Cell& cell = cells[cy][cx];

                cell.area.x_min = area.x_min + cx * CELL_SIZE;
                cell.area.x_max = (std::min)(cell.area.x_min + CELL_SIZE, area.x_max);

                cell.area.y_min = area.y_min + cy * CELL_SIZE;
                cell.area.y_max = (std::min)(cell.area.y_min + CELL_SIZE, area.y_max);

                cell.charSessions.reserve(CELL_CAPACITY);
                cell.monsterIndexes.reserve(CELL_CAPACITY);
                cell.dirtyChar.reserve(CELL_CAPACITY);
                cell.actionResults.reserve(CELL_CAPACITY);
            }
        }
    }

    inline std::pair<uint16_t, uint16_t> GetCell(float x, float y, const ZoneArea& area) {
        int cx = (x - area.x_min) / CELL_SIZE;
        int cy = (y - area.y_min) / CELL_SIZE;

        cx = std::clamp(cx, 0, CELLS_X - 1);
        cy = std::clamp(cy, 0, CELLS_Y - 1);

        return { (uint16_t)cx, (uint16_t)cy };
    }
}
