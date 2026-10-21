using System;
using System.Globalization;
using System.Collections.Generic;
using System.Text;
using C02;
using UnityEngine;

public static class C24WorldLayoutExport
{
    public static void Export(C20WorldMap map, C20MapAuthoring author, C24WorldLayout layout, string outputDirectory)
    {
        Validate(map, author, layout);
        C20WorldMapExport.ExportSnapshot(map, author, layout, outputDirectory);
    }

    internal static void AppendMetadata(StringBuilder text, C20MapAuthoring author, C24WorldLayout layout)
    {
        text.AppendFormat(CultureInfo.InvariantCulture,
            "inline constexpr int FormatVersion = 1;\ninline constexpr unsigned int ContentVersion = {0}U;\ninline constexpr int ZoneColumns = {1};\ninline constexpr int ZoneRows = {2};\ninline constexpr float ZoneCoreSize = {3:0.0########}f;\ninline constexpr float TransitionMargin = {4:0.0########}f;\ninline constexpr float ZoneOriginStep = {3:0.0########}f;\ninline constexpr float NavOriginX = {5:0.0########}f;\ninline constexpr float NavOriginY = {5:0.0########}f;\ninline constexpr float NavExtentX = {6:0.0########}f;\ninline constexpr float NavExtentY = {7:0.0########}f;\ninline constexpr bool BoundsAreHalfOpen = true;\n",
            author.contentVersion, layout.Columns, layout.Rows, layout.CoreSize, layout.TransitionMargin,
            -layout.TransitionMargin, layout.Columns * layout.CoreSize + 2 * layout.TransitionMargin,
            layout.Rows * layout.CoreSize + 2 * layout.TransitionMargin);
        text.Append("struct C24Rect { float x; float y; float width; float height; };\nstruct C24ZoneLayout { int id; int column; int row; float originX; float originY; float span; C24Rect localBounds; C24Rect ownershipCore; C24Rect exclusiveBounds; float aoiCellSize; std::array<C24Rect, 25> aoiCells; std::array<int, 8> neighborIds; std::array<C24Rect, 8> neighborIntersections; std::array<bool, 8> directCrossings; };\n");
        text.AppendFormat(CultureInfo.InvariantCulture, "inline constexpr std::array<C24ZoneLayout, {0}> Zones = {{{{\n", layout.Zones.Length);
        foreach (var zone in layout.Zones)
        {
            text.AppendFormat(CultureInfo.InvariantCulture,
                "{{{0}, {1}, {2}, {3:0.0########}f, {4:0.0########}f, {5:0.0########}f, ",
                zone.Id, zone.Column, zone.Row, zone.ZoneOrigin.x, zone.ZoneOrigin.y, zone.Span);
            AppendRect(text, zone.LocalBounds); text.Append(", ");
            AppendRect(text, zone.OwnershipCore); text.Append(", ");
            AppendRect(text, zone.ExclusiveBounds);
            text.AppendFormat(CultureInfo.InvariantCulture, ", {0:0.0########}f, {{{{", zone.AoiCellSize);
            for (int i = 0; i < zone.AoiCells.Length; i++) { if (i > 0) text.Append(", "); AppendRect(text, zone.AoiCells[i]); }
            text.Append("}}, {{");
            for (int i = 0; i < zone.NeighborIds.Length; i++) { if (i > 0) text.Append(", "); text.Append(zone.NeighborIds[i]); }
            text.Append("}}, {{");
            for (int i = 0; i < zone.NeighborIntersections.Length; i++) { if (i > 0) text.Append(", "); AppendRect(text, zone.NeighborIntersections[i]); }
            text.Append("}}, {{");
            for (int i = 0; i < zone.NeighborIds.Length; i++)
            {
                if (i > 0) text.Append(", ");
                bool open = false;
                if (zone.NeighborIds[i] != 0)
                {
                    var neighbor = layout.Zones[zone.NeighborIds[i] - 1];
                    Rect overlap = zone.NeighborIntersections[i]; overlap.position += zone.ZoneOrigin;
                    int dx = Math.Sign(neighbor.Column - zone.Column), dy = Math.Sign(neighbor.Row - zone.Row);
                    open = dx == 0 || dy == 0 || DirectCornerCrossing(author.map, overlap, dx, dy);
                }
                text.Append(open ? "true" : "false");
            }
            text.Append("}}},\n");
        }
        text.Append("}};\n");
        // Geometric adjacency remains even when directCrossings marks a blocked ownership corner.
        text.Append("inline constexpr bool DiagonalRequiresBothOrthogonalCells = true;\n");
        text.AppendFormat(CultureInfo.InvariantCulture, "inline constexpr float ActorClearance = {0:0.0########}f;\n", author.actorClearance);

        text.Append("struct C24ObstacleFootprint { std::array<float, 24> corners; float clearance; };\n");
        text.AppendFormat(CultureInfo.InvariantCulture,
            "inline constexpr std::array<C24ObstacleFootprint, {0}> ObstacleFootprints = {{{{\n",
            author.obstacles.Length);
        foreach (var obstacle in author.obstacles)
        {
            Bounds footprint = C20MapAuthoring.WorldBounds(obstacle);
            Vector3 half = footprint.extents;
            text.Append("{{{");
            for (int i = 0; i < 8; i++)
            {
                Vector3 p = footprint.center + Vector3.Scale(half,
                    new Vector3((i & 1) == 0 ? -1 : 1, (i & 2) == 0 ? -1 : 1, (i & 4) == 0 ? -1 : 1));
                if (i > 0) text.Append(", ");
                text.AppendFormat(CultureInfo.InvariantCulture, "{0:0.0########}f, {1:0.0########}f, {2:0.0########}f", p.x, p.y, p.z);
            }
            text.Append("}}, ");
            text.AppendFormat(CultureInfo.InvariantCulture, "{0:0.0########}f", author.actorClearance);
            text.Append("},\n");
        }
        text.Append("}};\n");
    }

    private static void Validate(C20WorldMap map, C20MapAuthoring author, C24WorldLayout layout)
    {
        if (map == null || author == null || layout == null || author.map != map || !map.IsValid)
            throw new InvalidOperationException("A valid baked navigation map, authoring root and complete layout are required.");
        if (author.contentVersion < 1 || layout.Columns < 1 || layout.Columns > 3 || layout.Rows < 1 || layout.Rows > 3 ||
            !Finite(layout.CoreSize) || !Finite(layout.TransitionMargin) || layout.CoreSize <= 0 || layout.TransitionMargin <= 0 ||
            layout.TransitionMargin * 2 >= layout.CoreSize)
            throw new InvalidOperationException("Unsupported layout format or dimensions.");
        float expectedWidth = layout.Columns * layout.CoreSize + 2 * layout.TransitionMargin;
        float expectedHeight = layout.Rows * layout.CoreSize + 2 * layout.TransitionMargin;
        if (!Near(map.origin.x, -layout.TransitionMargin) || !Near(map.origin.y, -layout.TransitionMargin) ||
            !Near(map.width * map.tileSize, expectedWidth) || !Near(map.height * map.tileSize, expectedHeight) ||
            !Near(layout.CoreSize / map.tileSize, Mathf.Round(layout.CoreSize / map.tileSize)) ||
            !Near(layout.TransitionMargin / map.tileSize, Mathf.Round(layout.TransitionMargin / map.tileSize)))
            throw new InvalidOperationException("Navigation origin, extent or tile alignment does not match the complete zone layout.");
        if (author.obstacles == null || !Finite(author.actorClearance) || author.actorClearance < 0)
            throw new InvalidOperationException("Explicit obstacle footprints and finite clearance are required.");
        foreach (var obstacle in author.obstacles)
            if (obstacle == null || obstacle == author.ground || !obstacle.transform.IsChildOf(author.transform))
                throw new InvalidOperationException("Every exported obstacle must be an explicit child BoxCollider.");
        foreach (var zone in layout.Zones)
            for (int i = 0; i < zone.NeighborIds.Length; i++)
            {
                int neighborId = zone.NeighborIds[i];
                if (neighborId <= zone.Id) continue;
                var neighbor = layout.Zones[neighborId - 1];
                Rect overlap = zone.NeighborIntersections[i];
                overlap.position += zone.ZoneOrigin;
                if ((neighbor.Column == zone.Column || neighbor.Row == zone.Row) &&
                    !HasConnection(map, overlap, Math.Sign(neighbor.Column - zone.Column), Math.Sign(neighbor.Row - zone.Row)))
                    throw new InvalidOperationException($"Zones {zone.Id} and {neighborId} have no traversable overlap connection.");
            }
    }

    // Export-time connectivity only; not monster pathfinding. Four-way edges avoid corner cutting.
    private static bool HasConnection(C20WorldMap map, Rect overlap, int dx, int dy)
    {
        float step = map.tileSize;
        int minX = Math.Max(0, Mathf.FloorToInt((overlap.xMin - (dx == 0 ? 0 : step) - map.origin.x) / step));
        int minY = Math.Max(0, Mathf.FloorToInt((overlap.yMin - (dy == 0 ? 0 : step) - map.origin.y) / step));
        int maxX = Math.Min(map.width - 1, Mathf.CeilToInt((overlap.xMax + (dx == 0 ? 0 : step) - map.origin.x) / step) - 1);
        int maxY = Math.Min(map.height - 1, Mathf.CeilToInt((overlap.yMax + (dy == 0 ? 0 : step) - map.origin.y) / step) - 1);
        int width = maxX - minX + 1, height = maxY - minY + 1;
        if (width <= 0 || height <= 0) return false;
        var visited = new bool[checked(width * height)];
        var pending = new Queue<int>();
        for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++)
            {
                bool entrance = (dx == 0 || x == (dx > 0 ? minX : maxX)) &&
                    (dy == 0 || y == (dy > 0 ? minY : maxY));
                if (!entrance || map.walkable[y * map.width + x] == 0) continue;
                int index = (y - minY) * width + x - minX;
                visited[index] = true;
                pending.Enqueue(index);
            }
        int[] offsetX = { -1, 1, 0, 0 }, offsetY = { 0, 0, -1, 1 };
        while (pending.Count > 0)
        {
            int index = pending.Dequeue();
            int x = minX + index % width, y = minY + index / width;
            if ((dx == 0 || x == (dx > 0 ? maxX : minX)) &&
                (dy == 0 || y == (dy > 0 ? maxY : minY))) return true;
            var from = new Vector2(map.origin.x + (x + .5f) * step, map.origin.y + (y + .5f) * step);
            for (int i = 0; i < 4; i++)
            {
                int nx = x + offsetX[i], ny = y + offsetY[i];
                if (nx < minX || nx > maxX || ny < minY || ny > maxY) continue;
                int next = (ny - minY) * width + nx - minX;
                if (visited[next] || map.walkable[ny * map.width + nx] == 0) continue;
                var to = new Vector2(map.origin.x + (nx + .5f) * step, map.origin.y + (ny + .5f) * step);
                if (!map.CanTraverse(from, to)) continue;
                visited[next] = true;
                pending.Enqueue(next);
            }
        }
        return false;
    }

    // Adjacency also drives scene visibility. A blocked ownership corner must not erase
    // the geometric neighbor or reject an otherwise connected world; export it explicitly.
    private static bool DirectCornerCrossing(C20WorldMap map, Rect overlap, int dx, int dy)
    {
        var corner = new Vector2(dx > 0 ? overlap.xMax : overlap.xMin, dy > 0 ? overlap.yMax : overlap.yMin);
        var offset = new Vector2(dx, dy) * (map.tileSize * .5f);
        return map.CanTraverse(corner - offset, corner + offset);
    }

    private static void AppendRect(StringBuilder text, Rect rect) =>
        text.AppendFormat(CultureInfo.InvariantCulture, "{{{0:0.0########}f, {1:0.0########}f, {2:0.0########}f, {3:0.0########}f}}", rect.x, rect.y, rect.width, rect.height);

    private static bool Near(float a, float b) => Mathf.Abs(a - b) <= .001f;
    private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
}
