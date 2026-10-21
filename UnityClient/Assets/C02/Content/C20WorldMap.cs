using System;
using UnityEngine;

namespace C02
{
    // The small navigation grid is separate from the server's AOI Cells.
    [CreateAssetMenu(menuName = "C02/World navigation map")]
    public sealed class C20WorldMap : ScriptableObject
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

        private bool Cell(int x, int y) => x >= 0 && y >= 0 && x < width && y < height && walkable[y * width + x] == 1;
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
