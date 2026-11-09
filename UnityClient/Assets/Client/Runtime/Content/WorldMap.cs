using System;
using System.Collections.Generic;
using UnityEngine;

namespace Mmorpg.Client
{
    // The small navigation grid is separate from the server's AOI Cells.
    [CreateAssetMenu(menuName = "Client/World Navigation Map")]
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C20WorldMap")]
    public sealed class WorldMap : ScriptableObject
    {
        public const float ZoneSize = 200;
        public Vector2 origin = new Vector2(-5, -5);
        public float tileSize = 1;
        public int width = 610;
        public int height = 410;
        public byte[] walkable = Array.Empty<byte>();

        public bool IsValid => width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
            Finite(origin.x) && Finite(origin.y) && Finite(tileSize) && tileSize > 0 &&
            walkable != null && (long)width * height == walkable.Length;

        public bool IsWalkable(float x, float y)
        {
            if (!IsValid || !Finite(x) || !Finite(y)) return false;
            double gx = ((double)x - origin.x) / tileSize, gy = ((double)y - origin.y) / tileSize;
            return gx >= 0 && gy >= 0 && gx < width && gy < height && Cell((int)Math.Floor(gx), (int)Math.Floor(gy));
        }

        public bool CanTraverse(Vector2 from, Vector2 to)
        {
            if (!IsWalkable(from.x, from.y) || !IsWalkable(to.x, to.y)) return false;
            double ax = ((double)from.x - origin.x) / tileSize, ay = ((double)from.y - origin.y) / tileSize;
            double bx = ((double)to.x - origin.x) / tileSize, by = ((double)to.y - origin.y) / tileSize;
            if (!Endpoint(ax, ay) || !Endpoint(bx, by)) return false;
            int x = (int)Math.Floor(ax), y = (int)Math.Floor(ay);
            double dx = bx - ax, dy = by - ay;
            int sx = Math.Sign(dx), sy = Math.Sign(dy);
            bool onVertical = dx == 0 && ax == Math.Floor(ax);
            bool onHorizontal = dy == 0 && ay == Math.Floor(ay);
            double tx = sx == 0 ? double.PositiveInfinity : ((sx > 0 ? x + 1 : x) - ax) / dx;
            double ty = sy == 0 ? double.PositiveInfinity : ((sy > 0 ? y + 1 : y) - ay) / dy;
            double stepX = sx == 0 ? double.PositiveInfinity : 1 / Math.Abs(dx);
            double stepY = sy == 0 ? double.PositiveInfinity : 1 / Math.Abs(dy);
            // Endpoints are already checked; avoid overshooting a mixed-sign end corner.
            while (tx < 1 || ty < 1)
            {
                if (Math.Abs(tx - ty) <= 1e-12)
                {
                    if (!Cell(x + sx, y) || !Cell(x, y + sy)) return false;
                    x += sx; y += sy; tx += stepX; ty += stepY;
                }
                else if (tx < ty) { x += sx; tx += stepX; }
                else { y += sy; ty += stepY; }
                if (!Cell(x, y) || onVertical && x > 0 && !Cell(x - 1, y) ||
                    onHorizontal && y > 0 && !Cell(x, y - 1)) return false;
            }
            return true;
        }

        private bool Endpoint(double x, double y)
        {
            int ix = (int)Math.Floor(x), iy = (int)Math.Floor(y);
            bool left = x == ix && ix > 0, bottom = y == iy && iy > 0;
            return Cell(ix, iy) && (!left || Cell(ix - 1, iy)) && (!bottom || Cell(ix, iy - 1)) &&
                (!left || !bottom || Cell(ix - 1, iy - 1));
        }

        // A map preview only. Every leg stays on the same grid used by local movement.
        public bool TryFindWalkingRoute(Vector2 start, Vector2 end, List<Vector2> route)
        {
            route.Clear();
            if (!IsWalkable(start.x, start.y) || !IsWalkable(end.x, end.y)) return false;
            if (CanTraverse(start, end))
            {
                route.Add(start);
                route.Add(end);
                return true;
            }

            int Index(Vector2 position) =>
                (int)Math.Floor(((double)position.y - origin.y) / tileSize) * width +
                (int)Math.Floor(((double)position.x - origin.x) / tileSize);
            Vector2 Centre(int index) => origin + new Vector2(index % width + .5f, index / width + .5f) * tileSize;
            int first = Index(start), last = Index(end);
            if (!CanTraverse(start, Centre(first)) || !CanTraverse(Centre(last), end)) return false;

            var parent = new int[walkable.Length];
            var frontier = new Queue<int>();
            parent[first] = -1;
            frontier.Enqueue(first);
            while (frontier.Count > 0 && parent[last] == 0)
            {
                int current = frontier.Dequeue();
                int x = current % width, y = current / width;
                Visit(x - 1, y, current);
                Visit(x + 1, y, current);
                Visit(x, y - 1, current);
                Visit(x, y + 1, current);
            }
            if (parent[last] == 0) return false;

            route.Add(end);
            route.Add(Centre(last));
            int next = last;
            int direction = 0;
            while (next != first)
            {
                int previous = parent[next] - 1;
                int step = next - previous;
                if (direction != 0 && direction != step) route.Add(Centre(next));
                direction = step;
                next = previous;
            }
            route.Add(Centre(first));
            route.Add(start);
            route.Reverse();
            return true;

            void Visit(int x, int y, int current)
            {
                if (!Cell(x, y)) return;
                int index = y * width + x;
                if (parent[index] != 0) return;
                parent[index] = current + 1;
                frontier.Enqueue(index);
            }
        }

        private bool Cell(int x, int y) => x >= 0 && y >= 0 && x < width && y < height && walkable[y * width + x] == 1;
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
