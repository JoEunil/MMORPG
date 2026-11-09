using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

namespace Mmorpg.Client
{
    internal sealed class WorldMapRouteGraphic : MaskableGraphic
    {
        private readonly List<Vector2> points = new List<Vector2>();

        internal void SetRoute(List<Vector2> route, Rect bounds)
        {
            points.Clear();
            foreach (var point in route)
                points.Add(new Vector2((point.x - bounds.xMin) / bounds.width, (point.y - bounds.yMin) / bounds.height));
            SetVerticesDirty();
        }

        protected override void OnPopulateMesh(VertexHelper vertices)
        {
            vertices.Clear();
            var rect = GetPixelAdjustedRect();
            DrawStroke(vertices, rect, 7, new Color32(12, 32, 38, 255));
            DrawStroke(vertices, rect, 3, new Color32(139, 229, 255, 255));
        }

        private void DrawStroke(VertexHelper vertices, Rect rect, float width, Color32 tint)
        {
            for (int i = 1; i < points.Count; i++)
            {
                Vector2 start = rect.min + Vector2.Scale(points[i - 1], rect.size);
                Vector2 end = rect.min + Vector2.Scale(points[i], rect.size);
                Vector2 direction = (end - start).normalized;
                if (direction == Vector2.zero) continue;
                Vector2 normal = new Vector2(-direction.y, direction.x) * (width * .5f);
                Vector2 cap = direction * (width * .5f);
                int index = vertices.currentVertCount;
                vertices.AddVert(start - cap - normal, tint, Vector2.zero);
                vertices.AddVert(start - cap + normal, tint, Vector2.zero);
                vertices.AddVert(end + cap + normal, tint, Vector2.zero);
                vertices.AddVert(end + cap - normal, tint, Vector2.zero);
                vertices.AddTriangle(index, index + 1, index + 2);
                vertices.AddTriangle(index, index + 2, index + 3);
            }
        }
    }
}
