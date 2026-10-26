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

        private const int TerrainSamples = 40;
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
            float size = Mathf.Min(rect.width, rect.height);
            if (size <= 0) return;
            var mapRect = new Rect(rect.center.x - size * .5f, rect.center.y - size * .5f, size, size);
            float cellSize = size / TerrainSamples;
            float worldStep = viewRadius * 2 / TerrainSamples;
            for (int y = 0; y < TerrainSamples; y++)
                for (int x = 0; x < TerrainSamples; x++)
                {
                    bool passable = worldMap.IsWalkable(centre.x - viewRadius + (x + .5f) * worldStep,
                        centre.y - viewRadius + (y + .5f) * worldStep);
                    Quad(vertices, mapRect.xMin + x * cellSize, mapRect.yMin + y * cellSize,
                        mapRect.xMin + (x + 1) * cellSize, mapRect.yMin + (y + 1) * cellSize,
                        passable ? WalkableColor : BlockedColor);
                }
            foreach (Vector2 position in players) Marker(vertices, mapRect, position, 4, PlayerColor);
            foreach (Vector2 position in monsters) Marker(vertices, mapRect, position, 4, MonsterColor);
            Marker(vertices, mapRect, centre, 6, SelfColor);
        }

        private void Marker(VertexHelper vertices, Rect rect, Vector2 position, float size, Color32 tint)
        {
            Vector2 offset = position - centre;
            if (Mathf.Abs(offset.x) > viewRadius || Mathf.Abs(offset.y) > viewRadius) return;
            Vector2 point = rect.center + offset * (rect.width / (viewRadius * 2));
            // Clip the dot itself at the viewport; never clamp an out-of-range actor onto an edge.
            Quad(vertices, Mathf.Max(rect.xMin, point.x - size * .5f), Mathf.Max(rect.yMin, point.y - size * .5f),
                Mathf.Min(rect.xMax, point.x + size * .5f), Mathf.Min(rect.yMax, point.y + size * .5f), tint);
        }

        private static void Quad(VertexHelper vertices, float left, float bottom, float right, float top, Color32 tint)
        {
            int start = vertices.currentVertCount;
            vertices.AddVert(new Vector3(left, bottom), tint, Vector2.zero);
            vertices.AddVert(new Vector3(left, top), tint, Vector2.zero);
            vertices.AddVert(new Vector3(right, top), tint, Vector2.zero);
            vertices.AddVert(new Vector3(right, bottom), tint, Vector2.zero);
            vertices.AddTriangle(start, start + 1, start + 2);
            vertices.AddTriangle(start, start + 2, start + 3);
        }
    }
}
