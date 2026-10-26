using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

namespace C02
{
    // North-up, player-centred presentation of the existing navigation map and accepted observations.
    public sealed class WorldMinimap : MaskableGraphic
    {
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private C04ObservedWorld observedWorld;
        [SerializeField] private C20WorldMap worldMap;
        [SerializeField] private CanvasGroup panelGroup;
        [SerializeField, Min(1)] private float viewRadius = 40;
        [SerializeField] private Texture2D terrainAtlas;
        [SerializeField] private Rect atlasBounds;

        public override Texture mainTexture => terrainAtlas != null ? terrainAtlas : Texture2D.whiteTexture;

        private const int TerrainSamples = 40;
        private const int CircleSegments = 64;
        private static readonly Vector2 UntexturedUv = new Vector2(-1, -1);
        private readonly Vector2[] polygon = new Vector2[80];
        private readonly Vector2[] clippedPolygon = new Vector2[80];
        private static readonly Color32 WalkableColor = new Color32(79, 91, 72, 235);
        private static readonly Color32 BlockedColor = new Color32(26, 31, 30, 245);
        private static readonly Color32 PlayerColor = new Color32(174, 181, 184, 255);
        private static readonly Color32 MonsterColor = new Color32(235, 74, 64, 255);
        private static readonly Color32 SelfColor = new Color32(74, 219, 115, 255);
        private readonly List<Vector2> players = new List<Vector2>();
        private readonly List<Vector2> monsters = new List<Vector2>();
        private Vector2 centre;
        private bool visible;
        private float nextRefresh;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull &&
            worldMap != null && worldMap.IsValid && viewRadius > 0 && !float.IsInfinity(viewRadius);

        protected override void OnEnable()
        {
            base.OnEnable();
            raycastTarget = false;
            if (panelGroup == null) panelGroup = GetComponentInParent<CanvasGroup>();
            if (worldScreen != null) worldScreen.WorldStateChanged += WorldChanged;
            nextRefresh = 0;
            Update();
        }

        private void WorldChanged()
        {
            if (!WorldAvailable) Hide();
        }

        private void Update()
        {
            if (!WorldAvailable || panelGroup == null) { Hide(); return; }
            panelGroup.blocksRaycasts = false;
            panelGroup.interactable = false;
            if (visible && Time.unscaledTime < nextRefresh) return;
            centre = worldScreen.ServerPosition;
            players.Clear();
            monsters.Clear();
            if (observedWorld != null) observedWorld.CopyMinimapPositions(players, monsters);
            visible = true;
            panelGroup.alpha = 1;
            nextRefresh = Time.unscaledTime + .1f;
            SetVerticesDirty();
        }

        private void Hide()
        {
            if (panelGroup != null) panelGroup.alpha = 0;
            players.Clear();
            monsters.Clear();
            nextRefresh = 0;
            if (!visible) return;
            visible = false;
            SetVerticesDirty();
        }

        protected override void OnDisable()
        {
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            Hide();
            base.OnDisable();
        }

        protected override void OnPopulateMesh(VertexHelper vertices)
        {
            vertices.Clear();
            if (!visible || !WorldAvailable) return;
            Rect rect = GetPixelAdjustedRect();
            float radius = Mathf.Min(rect.width, rect.height) * .5f;
            if (radius <= 0) return;
            Vector2 origin = rect.center;
            Circle(vertices, origin, radius, BlockedColor);
            if (terrainAtlas != null && atlasBounds.width > 0 && atlasBounds.height > 0)
            {
                for (int i = 0; i < CircleSegments; i++) polygon[i] = CirclePoint(origin, radius, i, CircleSegments);
                float scale = radius / viewRadius;
                int count = Clip(CircleSegments, Vector2.right, origin.x + (atlasBounds.xMax - centre.x) * scale);
                count = Clip(count, Vector2.left, -origin.x - (atlasBounds.xMin - centre.x) * scale);
                count = Clip(count, Vector2.up, origin.y + (atlasBounds.yMax - centre.y) * scale);
                count = Clip(count, Vector2.down, -origin.y - (atlasBounds.yMin - centre.y) * scale);
                Fill(vertices, count, new Color32(255, 255, 255, 255), origin, radius, true);
            }
            else DrawNavigation(vertices, origin, radius);

            Ring(vertices, origin, radius * .5f, .65f, new Color32(165, 173, 162, 90));
            Ring(vertices, origin, radius - .5f, .8f, new Color32(180, 187, 175, 150));
            Rectangle(origin.x - radius + 2, origin.y - .3f, origin.x + radius - 2, origin.y + .3f);
            Fill(vertices, 4, new Color32(164, 174, 160, 42), origin, radius, false);
            Rectangle(origin.x - .3f, origin.y - radius + 2, origin.x + .3f, origin.y + radius - 2);
            Fill(vertices, 4, new Color32(164, 174, 160, 42), origin, radius, false);
            foreach (Vector2 position in players) Marker(vertices, origin, radius, position, PlayerColor);
            foreach (Vector2 position in monsters) Marker(vertices, origin, radius, position, MonsterColor);
            Circle(vertices, origin, 7, new Color32(74, 219, 115, 55));
            polygon[0] = origin + Vector2.up * 5;
            polygon[1] = origin + Vector2.right * 5;
            polygon[2] = origin + Vector2.down * 5;
            polygon[3] = origin + Vector2.left * 5;
            Fill(vertices, 4, SelfColor, origin, radius, false);
        }

        private void DrawNavigation(VertexHelper vertices, Vector2 origin, float radius)
        {
            float cellSize = radius * 2 / TerrainSamples;
            float worldStep = viewRadius * 2 / TerrainSamples;
            for (int y = 0; y < TerrainSamples; y++)
                for (int x = 0; x < TerrainSamples; x++)
                {
                    Vector2 middle = new Vector2((x + .5f) * cellSize - radius, (y + .5f) * cellSize - radius);
                    if (middle.magnitude > radius + cellSize * .707107f) continue;
                    if (!worldMap.IsWalkable(centre.x - viewRadius + (x + .5f) * worldStep,
                        centre.y - viewRadius + (y + .5f) * worldStep)) continue;
                    Rectangle(origin.x + x * cellSize - radius, origin.y + y * cellSize - radius,
                        origin.x + (x + 1) * cellSize - radius, origin.y + (y + 1) * cellSize - radius);
                    int count = middle.magnitude <= radius - cellSize * .707107f ? 4 : ClipToCircle(4, origin, radius);
                    Fill(vertices, count, WalkableColor, origin, radius, false);
                }
        }

        private void Marker(VertexHelper vertices, Vector2 origin, float radius, Vector2 position, Color32 tint)
        {
            Vector2 offset = position - centre;
            if (offset.sqrMagnitude > viewRadius * viewRadius) return;
            Vector2 point = origin + offset * (radius / viewRadius);
            const float markerRadius = 3;
            for (int i = 0; i < 12; i++) polygon[i] = CirclePoint(point, markerRadius, i, 12);
            int count = offset.magnitude * (radius / viewRadius) + markerRadius <= radius
                ? 12 : ClipToCircle(12, origin, radius);
            Fill(vertices, count, tint, origin, radius, false);
        }

        private void Circle(VertexHelper vertices, Vector2 origin, float radius, Color32 tint)
        {
            for (int i = 0; i < CircleSegments; i++) polygon[i] = CirclePoint(origin, radius, i, CircleSegments);
            Fill(vertices, CircleSegments, tint, origin, radius, false);
        }

        private void Ring(VertexHelper vertices, Vector2 origin, float radius, float thickness, Color32 tint)
        {
            for (int i = 0; i < CircleSegments; i++)
            {
                polygon[0] = CirclePoint(origin, radius, i, CircleSegments);
                polygon[1] = CirclePoint(origin, radius, i + 1, CircleSegments);
                polygon[2] = CirclePoint(origin, radius - thickness, i + 1, CircleSegments);
                polygon[3] = CirclePoint(origin, radius - thickness, i, CircleSegments);
                Fill(vertices, 4, tint, origin, radius, false);
            }
        }

        private static Vector2 CirclePoint(Vector2 origin, float radius, int index, int segments)
        {
            float angle = index * (Mathf.PI * 2 / segments);
            return origin + new Vector2(Mathf.Cos(angle), Mathf.Sin(angle)) * radius;
        }

        private void Rectangle(float left, float bottom, float right, float top)
        {
            polygon[0] = new Vector2(left, bottom); polygon[1] = new Vector2(left, top);
            polygon[2] = new Vector2(right, top); polygon[3] = new Vector2(right, bottom);
        }

        private int ClipToCircle(int count, Vector2 origin, float radius)
        {
            // An inscribed polygon keeps every marker vertex inside the radar, including at its rim.
            float limit = radius * Mathf.Cos(Mathf.PI / CircleSegments);
            for (int i = 0; i < CircleSegments && count > 0; i++)
            {
                float angle = (i + .5f) * (Mathf.PI * 2 / CircleSegments);
                Vector2 normal = new Vector2(Mathf.Cos(angle), Mathf.Sin(angle));
                count = Clip(count, normal, Vector2.Dot(normal, origin) + limit);
            }
            return count;
        }

        private int Clip(int count, Vector2 normal, float limit)
        {
            if (count == 0) return 0;
            int result = 0;
            Vector2 previous = polygon[count - 1];
            float previousDistance = Vector2.Dot(normal, previous) - limit;
            for (int i = 0; i < count; i++)
            {
                Vector2 current = polygon[i];
                float distance = Vector2.Dot(normal, current) - limit;
                if ((distance <= 0) != (previousDistance <= 0))
                    clippedPolygon[result++] = Vector2.LerpUnclamped(previous, current,
                        previousDistance / (previousDistance - distance));
                if (distance <= 0) clippedPolygon[result++] = current;
                previous = current; previousDistance = distance;
            }
            System.Array.Copy(clippedPolygon, polygon, result);
            return result;
        }

        private void Fill(VertexHelper vertices, int count, Color32 tint, Vector2 origin, float radius, bool atlas)
        {
            if (count < 3) return;
            int start = vertices.currentVertCount;
            for (int i = 0; i < count; i++)
            {
                Vector2 uv = UntexturedUv;
                if (atlas)
                {
                    Vector2 world = centre + (polygon[i] - origin) * (viewRadius / radius);
                    uv = new Vector2(Mathf.Clamp01((world.x - atlasBounds.xMin) / atlasBounds.width),
                        Mathf.Clamp01((world.y - atlasBounds.yMin) / atlasBounds.height));
                }
                vertices.AddVert(polygon[i], tint, uv);
            }
            for (int i = 1; i < count - 1; i++) vertices.AddTriangle(start, start + i, start + i + 1);
        }
    }
}
