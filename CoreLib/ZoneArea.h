#pragma once
#include <array>
#include <cstdint>
#include <algorithm>

#include "Config.h"

namespace Core {
    inline constexpr const float ZONE_SIZE = 100.0f;     // zone 한 칸 크기
    inline constexpr const int ZONE_HORIZON = 3; // 3x3
    inline constexpr const int ZONE_VERTICAL = 3;
    inline constexpr const float TRANSITION_BUFFER = 5.0f; // 겹치는 영역

    static constexpr int CELL_SIZE = 22; // 5 * 5
    static constexpr int CELLS_X = (TRANSITION_BUFFER * 2 + ZONE_SIZE) / CELL_SIZE;
    static constexpr int CELLS_Y = (TRANSITION_BUFFER * 2 + ZONE_SIZE) / CELL_SIZE;
    static constexpr int CELL_CAPACITY = MAX_ZONE_CAPACITY / (CELLS_X * CELLS_Y) * 2;

    struct CellArea {
        float x_min, x_max;
        float y_min, y_max;
    };

    struct Cell {
        std::vector<uint64_t> charSessions;
        std::vector<uint16_t> monsterIndexes;
        std::vector<uint64_t> dirtyChar;   // Cell 단위 dirty list
        std::vector<uint16_t> dirtyMonster; 
        CellArea area;
    };

    struct ZoneArea {
        float x_min, x_max;
        float y_min, y_max;
    };

    inline void ZoneInit(ZoneArea& z, uint16_t zone) {
        int x = (zone - 1) % ZONE_HORIZON;
        int y = (zone - 1) / ZONE_HORIZON;
        z.x_min = x * ZONE_SIZE - TRANSITION_BUFFER;
        z.x_max = (x + 1) * ZONE_SIZE + TRANSITION_BUFFER;
        z.y_min = y * ZONE_SIZE - TRANSITION_BUFFER;
        z.y_max = (y + 1) * ZONE_SIZE + TRANSITION_BUFFER;
    }

    inline void CellInit(std::array<std::array<Cell, CELLS_X>, CELLS_Y>& cells, ZoneArea& area) {
        for (int cy = 0; cy < CELLS_Y; ++cy) {
            for (int cx = 0; cx < CELLS_X; ++cx) {

                Cell& cell = cells[cy][cx];

                cell.area.x_min = area.x_min + cx * CELL_SIZE;
                cell.area.x_max = cell.area.x_min + CELL_SIZE;

                cell.area.y_min = area.y_min + cy * CELL_SIZE;
                cell.area.y_max = cell.area.y_min + CELL_SIZE;

                cell.charSessions.reserve(CELL_CAPACITY);
                cell.monsterIndexes.reserve(CELL_CAPACITY);
                cell.dirtyChar.reserve(CELL_CAPACITY);
                cell.dirtyMonster.reserve(CELL_CAPACITY);
            }
        }
    }

    inline std::pair<uint16_t, uint16_t> GetCell(float x, float y, ZoneArea& area) {
        int cx = (x - area.x_min) / CELL_SIZE;
        int cy = (y - area.y_min) / CELL_SIZE;

        cx = std::clamp(cx, 0, CELLS_X - 1);
        cy = std::clamp(cy, 0, CELLS_Y - 1);

        return { (uint16_t)cx, (uint16_t)cy };
    }
}
