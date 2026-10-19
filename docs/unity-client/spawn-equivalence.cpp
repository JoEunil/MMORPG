// Compile this probe with the real CoreLib/ZoneState_Monsters.cpp and SkillData.cpp.
#include "CoreLib/pch.h"
#include "CoreLib/ZoneState.h"
#include <cstdio>
#include <cmath>

namespace Core {
    // Existing benchmark friend: inspect the actual initializer without product test hooks.
    class ZoneBenchmarkAccess {
    public:
        static bool CheckSpawns(const ZoneState& zone) {
            if (zone.m_monsters.size() != 241) return false;
            size_t referenced = 0;
            for (int y = 0; y < CELLS_Y; ++y) {
                for (int x = 0; x < CELLS_X; ++x) {
                    const auto& cell = zone.m_cells[y][x];
                    const bool center = x == 2 && y == 2;
                    if (cell.monsterIndexes.size() != (center ? 1 : 10)) return false;
                    for (size_t n = 0; n < cell.monsterIndexes.size(); ++n) {
                        const auto index = cell.monsterIndexes[n];
                        if (index >= zone.m_monsters.size()) return false;
                        const auto& monster = zone.m_monsters[index];
                        if (monster.internalID != index || monster.cellX != x || monster.cellY != y) return false;
                        if (monster.hp != monster.data->maxHp || monster.spawnX != monster.x || monster.spawnY != monster.y) return false;
                        if (center) {
                            if (index != 240 || monster.data->id != 2 ||
                                monster.x != (cell.area.x_min + cell.area.x_max) / 2 ||
                                monster.y != (cell.area.y_min + cell.area.y_max) / 2) return false;
                        } else {
                            if (index != referenced || monster.data->id != (n < 8 ? 0 : 1)) return false;
                            if (monster.x < cell.area.x_min + 1 || monster.x > cell.area.x_max - 1 ||
                                monster.y < cell.area.y_min + 1 || monster.y > cell.area.y_max - 1 ||
                                std::floor(monster.x) != monster.x || std::floor(monster.y) != monster.y) return false;
                            ++referenced;
                        }
                    }
                }
            }
            return referenced == 240;
        }
    };
}

int main() {
    auto zone = std::make_unique<Core::ZoneState>(1);
    zone->InitializeMonster();
    if (!Core::ZoneBenchmarkAccess::CheckSpawns(*zone)) {
        std::fputs("FAIL: actual monster spawn count/order/IDs/positions differ from the existing layout.\n", stderr);
        return 1;
    }
    std::puts("PASS: actual InitializeMonster created 241 monsters; 24 cells retain 8+2 ordered types and integer offsets 1..21; center boss ID240.");
    return 0;
}