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
        [SerializeField, Min(1), Tooltip("World metres from the centre to either horizontal edge.")] private float viewRadius = 40;
        [SerializeField] private Texture2D terrainAtlas;
        [SerializeField] private Rect atlasBounds;

        public override Texture mainTexture => terrainAtlas != null ? terrainAtlas : Texture2D.whiteTexture;

        private const int TerrainSamples = 40;
        private const int CircleSegments = 64;
        private static readonly Vector2 UntexturedUv = new Vector2(-1, -1);
        private readonly Vector2[] polygon = new Vector2[80];
        private readonly Vector2[] clippedPolygon = new Vector2[80];
        private static readonly Color32 WalkableColor = new Color32(59, 91, 93, 235);
        private static readonly Color32 BlockedColor = new Color32(19, 48, 57, 245);
        private static readonly Color32 PlayerColor = new Color32(174, 181, 184, 255);
        private static readonly Color32 MonsterColor = new Color32(235, 74, 64, 255);
        private static readonly Color32 SelfColor = new Color32(74, 219, 115, 255);
        private readonly List<Vector2> players = new List<Vector2>();
        private readonly List<Vector2> monsters = new List<Vector2>();
        private Vector2 centre;
        private bool visible;
        private float nextRefresh;
        private Rect meshBounds;

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
            meshBounds = rect;
            float radius = rect.width * .5f;
            if (radius <= 0 || rect.height <= 0) return;
            Vector2 origin = rect.center;
            Rectangle(rect.xMin, rect.yMin, rect.xMax, rect.yMax);
            Fill(vertices, 4, BlockedColor, origin, radius, false);
            if (terrainAtlas != null && atlasBounds.width > 0 && atlasBounds.height > 0)
            {
                Rectangle(rect.xMin, rect.yMin, rect.xMax, rect.yMax);
                float scale = radius / viewRadius;
                int count = Clip(4, Vector2.right, origin.x + (atlasBounds.xMax - centre.x) * scale);
                count = Clip(count, Vector2.left, -origin.x - (atlasBounds.xMin - centre.x) * scale);
                count = Clip(count, Vector2.up, origin.y + (atlasBounds.yMax - centre.y) * scale);
                count = Clip(count, Vector2.down, -origin.y - (atlasBounds.yMin - centre.y) * scale);
                Fill(vertices, count, new Color32(255, 255, 255, 255), origin, radius, true);
            }
            else DrawNavigation(vertices, rect);

            Ring(vertices, rect, radius * (20 / viewRadius), .65f, new Color32(135, 183, 176, 125));
            Ring(vertices, rect, radius * (40 / viewRadius) - .5f, .8f, new Color32(135, 183, 176, 175));
            Rectangle(rect.xMin + 2, origin.y - .3f, rect.xMax - 2, origin.y + .3f);
            Fill(vertices, 4, new Color32(164, 174, 160, 42), origin, radius, false);
            Rectangle(origin.x - .3f, rect.yMin + 2, origin.x + .3f, rect.yMax - 2);
            Fill(vertices, 4, new Color32(164, 174, 160, 42), origin, radius, false);
            foreach (Vector2 position in players) Marker(vertices, rect, position, PlayerColor);
            foreach (Vector2 position in monsters) Marker(vertices, rect, position, MonsterColor);
            Circle(vertices, origin, 8.2f, new Color32(112, 228, 154, 51));
            polygon[0] = origin + Vector2.up * 6.2f;
            polygon[1] = origin + Vector2.right * 6.2f;
            polygon[2] = origin + Vector2.down * 6.2f;
            polygon[3] = origin + Vector2.left * 6.2f;
            Fill(vertices, 4, new Color32(239, 255, 220, 255), origin, radius, false);
            polygon[0] = origin + Vector2.up * 5;
            polygon[1] = origin + Vector2.right * 5;
            polygon[2] = origin + Vector2.down * 5;
            polygon[3] = origin + Vector2.left * 5;
            Fill(vertices, 4, SelfColor, origin, radius, false);
        }

        private void DrawNavigation(VertexHelper vertices, Rect rect)
        {
            float cellWidth = rect.width / TerrainSamples;
            float cellHeight = rect.height / TerrainSamples;
            float metresPerPixel = viewRadius * 2 / rect.width;
            for (int y = 0; y < TerrainSamples; y++)
                for (int x = 0; x < TerrainSamples; x++)
                {
                    Vector2 offset = new Vector2((x + .5f) * cellWidth - rect.width * .5f,
                        (y + .5f) * cellHeight - rect.height * .5f);
                    if (!worldMap.IsWalkable(centre.x + offset.x * metresPerPixel,
                        centre.y + offset.y * metresPerPixel)) continue;
                    Rectangle(rect.xMin + x * cellWidth, rect.yMin + y * cellHeight,
                        rect.xMin + (x + 1) * cellWidth, rect.yMin + (y + 1) * cellHeight);
                    Fill(vertices, 4, WalkableColor, rect.center, rect.width * .5f, false);
                }
        }

        private void Marker(VertexHelper vertices, Rect rect, Vector2 position, Color32 tint)
        {
            Vector2 point = rect.center + (position - centre) * (rect.width / (viewRadius * 2));
            if (point.x < rect.xMin || point.x > rect.xMax || point.y < rect.yMin || point.y > rect.yMax) return;
            for (int i = 0; i < 12; i++) polygon[i] = CirclePoint(point, 5.3f, i, 12);
            Fill(vertices, ClipToRect(12, rect), new Color32(239, 247, 222, 180), rect.center, rect.width * .5f, false);
            for (int i = 0; i < 12; i++) polygon[i] = CirclePoint(point, 4.5f, i, 12);
            Fill(vertices, ClipToRect(12, rect), new Color32(11, 25, 30, 255), rect.center, rect.width * .5f, false);
            for (int i = 0; i < 12; i++) polygon[i] = CirclePoint(point, 3, i, 12);
            Fill(vertices, ClipToRect(12, rect), tint, rect.center, rect.width * .5f, false);
        }

        private void Circle(VertexHelper vertices, Vector2 origin, float radius, Color32 tint)
        {
            for (int i = 0; i < CircleSegments; i++) polygon[i] = CirclePoint(origin, radius, i, CircleSegments);
            Fill(vertices, CircleSegments, tint, origin, radius, false);
        }

        private void Ring(VertexHelper vertices, Rect rect, float radius, float thickness, Color32 tint)
        {
            Vector2 origin = rect.center;
            for (int i = 0; i < CircleSegments; i++)
            {
                polygon[0] = CirclePoint(origin, radius, i, CircleSegments);
                polygon[1] = CirclePoint(origin, radius, i + 1, CircleSegments);
                polygon[2] = CirclePoint(origin, radius - thickness, i + 1, CircleSegments);
                polygon[3] = CirclePoint(origin, radius - thickness, i, CircleSegments);
                Fill(vertices, ClipToRect(4, rect), tint, origin, rect.width * .5f, false);
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

        private int ClipToRect(int count, Rect rect)
        {
            count = Clip(count, Vector2.right, rect.xMax);
            count = Clip(count, Vector2.left, -rect.xMin);
            count = Clip(count, Vector2.up, rect.yMax);
            return Clip(count, Vector2.down, -rect.yMin);
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
                var vertex = UIVertex.simpleVert;
                vertex.position = polygon[i];
                vertex.color = tint;
                // Atlas UVs and overlay sentinels stay in xy; zw carries the same edge mask for every layer.
                vertex.uv0 = new Vector4(uv.x, uv.y,
                    (polygon[i].x - meshBounds.xMin) / meshBounds.width,
                    (polygon[i].y - meshBounds.yMin) / meshBounds.height);
                vertices.AddVert(vertex);
            }
            for (int i = 1; i < count - 1; i++) vertices.AddTriangle(start, start + i, start + i + 1);
        }
    }
}
