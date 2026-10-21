#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Core::WorldMap {
#include "Generated/WorldMap.inc"

    static_assert(Width > 0 && Height > 0 && TileSize > 0.0f);

    namespace Detail {
        inline bool IsWalkableTile(int x, int y, const decltype(WalkableBits)& bits) {
            if (x < 0 || y < 0 || x >= Width || y >= Height)
                return false;
            const auto index = static_cast<std::size_t>(y) * Width + x;
            return (bits[index / 64] & (uint64_t{1} << (index % 64))) != 0;
        }

        inline bool InBounds(float x, float y) {
            return std::isfinite(x) && std::isfinite(y)
                && x >= OriginX && y >= OriginY
                && static_cast<double>(x) < OriginX + static_cast<double>(Width) * TileSize
                && static_cast<double>(y) < OriginY + static_cast<double>(Height) * TileSize;
        }

        // A closed segment touches both sides of an internal gridline, including its endpoints.
        // Outside neighbors at the world minimum are ignored; the maximum itself is out of bounds.
        inline bool IsWalkableEndpoint(double x, double y, const decltype(WalkableBits)& bits) {
            const int tileX = static_cast<int>(std::floor(x));
            const int tileY = static_cast<int>(std::floor(y));
            const bool touchesLeft = tileX > 0 && x == tileX;
            const bool touchesBelow = tileY > 0 && y == tileY;
            return IsWalkableTile(tileX, tileY, bits)
                && (!touchesLeft || IsWalkableTile(tileX - 1, tileY, bits))
                && (!touchesBelow || IsWalkableTile(tileX, tileY - 1, bits))
                && (!touchesLeft || !touchesBelow || IsWalkableTile(tileX - 1, tileY - 1, bits));
        }

        inline bool CanTraverse(float fromX, float fromY, float toX, float toY,
            const decltype(WalkableBits)& bits) {
            if (!InBounds(fromX, fromY) || !InBounds(toX, toY))
                return false;
            const double startX = (static_cast<double>(fromX) - OriginX) / TileSize;
            const double startY = (static_cast<double>(fromY) - OriginY) / TileSize;
            const double endX = (static_cast<double>(toX) - OriginX) / TileSize;
            const double endY = (static_cast<double>(toY) - OriginY) / TileSize;
            if (!IsWalkableEndpoint(startX, startY, bits) || !IsWalkableEndpoint(endX, endY, bits))
                return false;

            int x = static_cast<int>(std::floor(startX));
            int y = static_cast<int>(std::floor(startY));
            const double dx = endX - startX;
            const double dy = endY - startY;
            const int stepX = (dx > 0) - (dx < 0);
            const int stepY = (dy > 0) - (dy < 0);
            const double infinity = (std::numeric_limits<double>::infinity)();
            const double deltaX = stepX == 0 ? infinity : 1.0 / std::abs(dx);
            const double deltaY = stepY == 0 ? infinity : 1.0 / std::abs(dy);
            double crossingX = stepX == 0 ? infinity : ((stepX > 0 ? x + 1.0 : x) - startX) / dx;
            double crossingY = stepY == 0 ? infinity : ((stepY > 0 ? y + 1.0 : y) - startY) / dy;
            const bool followsVerticalEdge = stepX == 0 && x > 0 && startX == x;
            const bool followsHorizontalEdge = stepY == 0 && y > 0 && startY == y;

            // Endpoints were checked separately, so only cross boundaries before t == 1.
            while (crossingX < 1.0 || crossingY < 1.0) {
                if (std::abs(crossingX - crossingY) <= 1e-12) {
                    // A diagonal may not squeeze between two touching blocked tiles.
                    if (!IsWalkableTile(x + stepX, y, bits) || !IsWalkableTile(x, y + stepY, bits))
                        return false;
                    x += stepX;
                    y += stepY;
                    crossingX += deltaX;
                    crossingY += deltaY;
                }
                else if (crossingX < crossingY) {
                    x += stepX;
                    crossingX += deltaX;
                }
                else {
                    y += stepY;
                    crossingY += deltaY;
                }
                if (!IsWalkableTile(x, y, bits)
                    || (followsVerticalEdge && !IsWalkableTile(x - 1, y, bits))
                    || (followsHorizontalEdge && !IsWalkableTile(x, y - 1, bits)))
                    return false;
            }
            return true;
        }
    }

    // Fine navigation tiles are independent of the coarse Zone/Cell AOI grid.
    inline bool IsWalkable(float x, float y) {
        if (!Detail::InBounds(x, y))
            return false;
        return Detail::IsWalkableTile(static_cast<int>(std::floor((static_cast<double>(x) - OriginX) / TileSize)),
            static_cast<int>(std::floor((static_cast<double>(y) - OriginY) / TileSize)), WalkableBits);
    }

    inline bool CanTraverse(float fromX, float fromY, float toX, float toY) {
        return Detail::CanTraverse(fromX, fromY, toX, toY, WalkableBits);
    }
}
