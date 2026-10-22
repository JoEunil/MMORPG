using System;
using UnityEngine;

namespace C02
{
    // The 5x5 AOI partition is independent of the navigation tile grid.
    public sealed class C24WorldLayout
    {
        public int Columns { get; }
        public int Rows { get; }
        public float CoreSize { get; }
        public float TransitionMargin { get; }
        public Zone[] Zones { get; }

        public struct Zone
        {
            public int Id;
            public int Column;
            public int Row;
            public Vector2 ZoneOrigin;
            public float Span;
            public Rect LocalBounds;
            public Rect OwnershipCore;
            public Rect ExclusiveBounds;
            public float AoiCellSize;
            public Rect[] AoiCells;
            // bottom-left, bottom, bottom-right, left, right, top-left, top, top-right
            public int[] NeighborIds;
            public Rect[] NeighborIntersections;
        }

        private C24WorldLayout(int columns, int rows, float coreSize, float transitionMargin, Zone[] zones)
        {
            Columns = columns;
            Rows = rows;
            CoreSize = coreSize;
            TransitionMargin = transitionMargin;
            Zones = zones;
        }

        public static C24WorldLayout Build(int columns, int rows, float coreSize, float transitionMargin)
        {
            if (columns < 1 || columns > 3 || rows < 1 || rows > 3 ||
                !Finite(coreSize) || !Finite(transitionMargin) || coreSize <= 0 || transitionMargin <= 0 ||
                transitionMargin * 2 >= coreSize)
                throw new ArgumentOutOfRangeException("Invalid world layout dimensions or transition margin.");

            float span = coreSize + 2 * transitionMargin;
            if (!Finite(span)) throw new ArgumentOutOfRangeException(nameof(coreSize), "World layout span must be finite.");
            var zones = new Zone[columns * rows];
            for (int row = 0; row < rows; row++)
                for (int column = 0; column < columns; column++)
                {
                    int id = row * columns + column + 1;
                    var origin = new Vector2(column * coreSize - transitionMargin, row * coreSize - transitionMargin);
                    var localBounds = new Rect(0, 0, span, span);
                    var ownership = new Rect(transitionMargin, transitionMargin, coreSize, coreSize);
                    float minX = column > 0 ? transitionMargin * 2 : 0;
                    float maxX = column + 1 < columns ? coreSize : span;
                    float minY = row > 0 ? transitionMargin * 2 : 0;
                    float maxY = row + 1 < rows ? coreSize : span;
                    var exclusive = Rect.MinMaxRect(minX, minY, maxX, maxY);
                    float aoiCellSize = span / 5;
                    var aoiCells = new Rect[25];
                    for (int y = 0; y < 5; y++)
                        for (int x = 0; x < 5; x++)
                            aoiCells[y * 5 + x] = new Rect(x * aoiCellSize, y * aoiCellSize, aoiCellSize, aoiCellSize);

                    var neighborIds = new int[8];
                    var intersections = new Rect[8];
                    int[] dx = { -1, 0, 1, -1, 1, -1, 0, 1 };
                    int[] dy = { -1, -1, -1, 0, 0, 1, 1, 1 };
                    for (int i = 0; i < 8; i++)
                    {
                        int nc = column + dx[i], nr = row + dy[i];
                        if (nc < 0 || nc >= columns || nr < 0 || nr >= rows) continue;
                        neighborIds[i] = nr * columns + nc + 1;
                        float ix0 = Mathf.Max(origin.x, nc * coreSize - transitionMargin);
                        float iy0 = Mathf.Max(origin.y, nr * coreSize - transitionMargin);
                        float ix1 = Mathf.Min(origin.x + span, nc * coreSize + coreSize + transitionMargin);
                        float iy1 = Mathf.Min(origin.y + span, nr * coreSize + coreSize + transitionMargin);
                        intersections[i] = Rect.MinMaxRect(ix0 - origin.x, iy0 - origin.y, ix1 - origin.x, iy1 - origin.y);
                    }
                    zones[id - 1] = new Zone
                    {
                        Id = id, Column = column, Row = row, ZoneOrigin = origin, Span = span,
                        LocalBounds = localBounds, OwnershipCore = ownership, ExclusiveBounds = exclusive,
                        AoiCellSize = aoiCellSize, AoiCells = aoiCells,
                        NeighborIds = neighborIds, NeighborIntersections = intersections
                    };
                }
            return new C24WorldLayout(columns, rows, coreSize, transitionMargin, zones);
        }

        public bool WorldToLocal(int zoneId, Vector2 world, out Vector2 local)
        {
            local = default;
            if (!TryZone(zoneId, out var zone) || !Finite(world.x) || !Finite(world.y)) return false;
            local = world - zone.ZoneOrigin;
            return ContainsHalfOpen(zone.LocalBounds, local);
        }

        public bool LocalToWorld(int zoneId, Vector2 local, out Vector2 world)
        {
            world = default;
            if (!TryZone(zoneId, out var zone) || !Finite(local.x) || !Finite(local.y) || !ContainsHalfOpen(zone.LocalBounds, local)) return false;
            world = local + zone.ZoneOrigin;
            return true;
        }

        private bool TryZone(int id, out Zone zone)
        {
            if (id > 0 && id <= Zones.Length) { zone = Zones[id - 1]; return true; }
            zone = default;
            return false;
        }

        private static bool ContainsHalfOpen(Rect rect, Vector2 point) =>
            point.x >= rect.xMin && point.x < rect.xMax && point.y >= rect.yMin && point.y < rect.yMax;

        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
