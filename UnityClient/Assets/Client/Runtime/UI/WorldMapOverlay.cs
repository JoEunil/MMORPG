using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace Mmorpg.Client
{
    // A north-up overview uses the same world coordinates and atlas as the minimap.
    internal sealed class WorldMapOverlay
    {
        private static readonly Color Gold = new Color32(235, 208, 145, 255);
        private static readonly Color White = new Color32(244, 239, 222, 255);
        private readonly Rect bounds;
        private readonly RectTransform root;
        private readonly RectTransform marker;
        private readonly TMP_Text location;
        private readonly TMP_Text routeStatus;
        private readonly TMP_Text fieldStatus;
        private readonly GameObject fieldBadge;
        private readonly MapAuthoring mapAuthor;
        private readonly WorldMap navigation;
        private readonly WorldMapRouteGraphic routeGraphic;
        private readonly List<Vector2> route = new List<Vector2>();
        private readonly Dictionary<int, UnityEngine.UI.Image> zones = new Dictionary<int, UnityEngine.UI.Image>();
        private readonly ServerContent content;
        private int displayedZone = -1;
        private Vector2 currentPosition;
        private Vector2 destination;
        private int destinationZone;
        private string destinationName;
        private WorldRouteGuide fieldGuide;
        private Vector2 lastRouteOrigin;
        private float nextRouteSearch;
        private bool refreshField = true;

        internal bool IsOpen => root != null && root.gameObject.activeSelf;

        internal WorldMapOverlay(Transform canvas, Texture2D atlas, Rect bounds, MapAuthoring author,
            ServerContent content, TMP_FontAsset font, System.Action close)
        {
            this.bounds = bounds;
            this.content = content;
            navigation = author.map;
            mapAuthor = author;
            var shade = Surface(canvas, "WorldMapOverlay", new Color32(12, 17, 19, 247));
            root = shade.rectTransform;
            Stretch(root, Vector2.zero, Vector2.zero);
            shade.raycastTarget = true;

            var title = Label(root, "Title", "월드 지도", 32, Gold, TextAlignmentOptions.MidlineLeft, font);
            Anchor(title.rectTransform, new Vector2(0, 1), new Vector2(0, 1), new Vector2(34, -33), new Vector2(280, 44));
            title.fontStyle = FontStyles.Bold;
            var closeFace = Surface(root, "Close", new Color32(50, 58, 56, 255));
            Anchor(closeFace.rectTransform, Vector2.one, Vector2.one, new Vector2(-30, -30), new Vector2(106, 36));
            closeFace.raycastTarget = true;
            var closeButton = closeFace.gameObject.AddComponent<UnityEngine.UI.Button>();
            closeButton.targetGraphic = closeFace;
            closeButton.onClick.AddListener(() => close());
            Stretch(Label(closeFace.transform, "Label", "닫기", 18, White, TextAlignmentOptions.Center, font).rectTransform,
                Vector2.zero, Vector2.zero);

            var area = CreateRect(root, "MapArea");
            Stretch(area, new Vector2(34, 64), new Vector2(-34, -64));
            var imageRect = CreateRect(area, "Terrain");
            var terrain = imageRect.gameObject.AddComponent<UnityEngine.UI.RawImage>();
            terrain.texture = atlas;
            terrain.raycastTarget = false;
            var fit = imageRect.gameObject.AddComponent<AspectRatioFitter>();
            fit.aspectRatio = bounds.width / bounds.height;
            fit.aspectMode = AspectRatioFitter.AspectMode.FitInParent;

            var layout = WorldLayout.Build(author.zoneColumns, author.zoneRows, author.zoneCoreSize, author.transitionMargin);
            foreach (var zone in layout.Zones)
            {
                var core = zone.OwnershipCore;
                var fill = Surface(imageRect, "Zone" + zone.Id, Color.clear);
                var zoneRect = fill.rectTransform;
                zoneRect.anchorMin = MapPoint(zone.ZoneOrigin + core.min);
                zoneRect.anchorMax = MapPoint(zone.ZoneOrigin + core.max);
                zoneRect.offsetMin = zoneRect.offsetMax = Vector2.zero;
                zones.Add(zone.Id, fill);
                var edgeColor = new Color32(242, 220, 159, 152);
                foreach (bool max in new[] { false, true })
                {
                    float edge = max ? 1 : 0;
                    var line = Surface(zoneRect, "HorizontalBoundary", edgeColor).rectTransform;
                    line.anchorMin = new Vector2(0, edge); line.anchorMax = new Vector2(1, edge);
                    line.sizeDelta = new Vector2(0, 1); line.anchoredPosition = Vector2.zero;
                    line = Surface(zoneRect, "VerticalBoundary", edgeColor).rectTransform;
                    line.anchorMin = new Vector2(edge, 0); line.anchorMax = new Vector2(edge, 1);
                    line.sizeDelta = new Vector2(1, 0); line.anchoredPosition = Vector2.zero;
                }

                var data = content == null ? null : content.GetHuntingZone(zone.Id);
                string areaName = data == null ? "구역 " + zone.Id : data.label;
                string level = data == null ? "" : zone.Id == 1 ? "안전 구역" : "권장 Lv. " + data.minLevel + "–" + data.maxLevel;
                var badge = Surface(zoneRect, "AreaLabel", new Color32(18, 25, 25, 225));
                badge.rectTransform.anchorMin = new Vector2(.1f, .5f);
                badge.rectTransform.anchorMax = new Vector2(.9f, .5f);
                badge.rectTransform.sizeDelta = new Vector2(0, 52);
                badge.rectTransform.anchoredPosition = Vector2.zero;
                var label = Label(badge.transform, "NameAndLevel", areaName + "\n" + level, 20, White,
                    TextAlignmentOptions.Center, font);
                label.enableAutoSizing = true;
                label.fontSizeMin = 11;
                label.fontSizeMax = 20;
                Stretch(label.rectTransform, new Vector2(4, 2), new Vector2(-4, -2));
            }

            AddTownRoute(imageRect, author, layout, font);
            AddHuntingDestinations(imageRect, author, layout, font);
            routeGraphic = CreateRect(imageRect, "SelectedWalkingRoute").gameObject.AddComponent<WorldMapRouteGraphic>();
            Stretch(routeGraphic.rectTransform, Vector2.zero, Vector2.zero);
            routeGraphic.raycastTarget = false;
            var north = Label(imageRect, "North", "N ↑", 19, Gold, TextAlignmentOptions.Center, font);
            Anchor(north.rectTransform, Vector2.one, Vector2.one, new Vector2(-8, -6), new Vector2(50, 26));
            marker = Surface(imageRect, "CurrentPosition", White).rectTransform;
            marker.sizeDelta = new Vector2(14, 14);
            marker.localRotation = Quaternion.Euler(0, 0, 45);
            Stretch(Surface(marker, "Pin", new Color32(48, 227, 108, 255)).rectTransform,
                new Vector2(2, 2), new Vector2(-2, -2));
            location = Label(root, "Location", "", 19, White, TextAlignmentOptions.MidlineLeft, font);
            Anchor(location.rectTransform, Vector2.zero, Vector2.zero, new Vector2(34, 16), new Vector2(410, 32));
            routeStatus = Label(root, "RouteStatus", "목표 지점을 클릭해 길 보기", 17, White, TextAlignmentOptions.MidlineRight, font);
            Anchor(routeStatus.rectTransform, Vector2.right, Vector2.right, new Vector2(-34, 16), new Vector2(340, 32));
            var field = Surface(canvas, "FieldRouteStatus", new Color32(15, 37, 45, 235));
            Anchor(field.rectTransform, new Vector2(.5f, 1), new Vector2(.5f, 1), new Vector2(0, -68), new Vector2(350, 32));
            fieldStatus = Label(field.transform, "Destination", "", 17, new Color32(169, 240, 255, 255),
                TextAlignmentOptions.Center, font);
            Stretch(fieldStatus.rectTransform, new Vector2(8, 0), new Vector2(-97, 0));
            var cancelFace = Surface(field.transform, "CancelNavigation", new Color32(39, 73, 81, 255));
            Anchor(cancelFace.rectTransform, new Vector2(1, .5f), new Vector2(1, .5f),
                new Vector2(-3, 0), new Vector2(90, 26));
            cancelFace.raycastTarget = true;
            var cancel = cancelFace.gameObject.AddComponent<UnityEngine.UI.Button>();
            cancel.targetGraphic = cancelFace;
            cancel.onClick.AddListener(() => SuspendNavigation(true));
            var cancelLabel = Label(cancelFace.transform, "Label", "안내 끄기", 15, White, TextAlignmentOptions.Center, font);
            Stretch(cancelLabel.rectTransform, Vector2.zero, Vector2.zero);
            fieldBadge = field.gameObject;
            fieldBadge.SetActive(false);
            root.gameObject.SetActive(false);
        }

        private Vector2 MapPoint(Vector2 position) => new Vector2(
            Mathf.InverseLerp(bounds.xMin, bounds.xMax, position.x),
            Mathf.InverseLerp(bounds.yMin, bounds.yMax, position.y));

        private void AddTownRoute(RectTransform terrain, MapAuthoring author, WorldLayout layout, TMP_FontAsset font)
        {
            var citadel = author.transform.parent?.Find("CitadelEnvironment");
            if (citadel == null || !citadel.gameObject.activeInHierarchy) return;

            foreach (Transform module in citadel)
                if (module.name.StartsWith("WesternCitadel", System.StringComparison.Ordinal) ||
                    module.name.StartsWith("SouthernCitadel", System.StringComparison.Ordinal))
                    AddWallFootprint(terrain, module);
            var corner = citadel.Find("TownContinuity");
            if (corner != null)
            {
                AddWallFootprint(terrain, corner.Find("WesternCorner"));
                AddWallFootprint(terrain, corner.Find("SouthernCorner"));
            }

            // The existing southern gate is outside the playable ground; it is a landmark, not the field crossing.
            var gate = citadel.Find("SouthernCitadel1");
            if (gate != null && gate.gameObject.activeInHierarchy)
            {
                Vector2 position = MapPoint(new Vector2(gate.position.x, gate.position.z));
                var pin = Surface(terrain, "SouthGateMarker", White).rectTransform;
                Anchor(pin, position, new Vector2(.5f, .5f), Vector2.zero, new Vector2(9, 9));
                pin.localRotation = Quaternion.Euler(0, 0, 45);
                RouteLabel(terrain, "SouthGateLabel", "남쪽 성문 · 경관", position, new Vector2(10, 0), font);
            }

            if (layout.Columns < 2 || author.map == null) return;
            var road = citadel.Find("TownRoadApproach/FieldboundCobbles_1")?.GetComponent<MeshRenderer>();
            if (road == null || !road.enabled || !road.gameObject.activeInHierarchy) return;

            // FieldDressingSetup extends the starter cobbles along z=10 through the first zone overlap.
            var start = new Vector2(10, 10);
            var end = new Vector2(road.bounds.max.x, start.y);
            float crossing = layout.Zones[1].ZoneOrigin.x + layout.Zones[1].OwnershipCore.xMin;
            if (end.x < crossing || !author.map.CanTraverse(start, end)) return;
            Vector2 from = MapPoint(start), to = MapPoint(end);
            var outline = Surface(terrain, "FieldPassageOutline", new Color32(25, 26, 21, 255)).rectTransform;
            outline.anchorMin = from;
            outline.anchorMax = to;
            outline.anchoredPosition = Vector2.zero;
            outline.sizeDelta = new Vector2(0, 8);
            var strip = Surface(outline, "FieldPassage", Gold).rectTransform;
            Stretch(strip, new Vector2(0, 2), new Vector2(0, -2));
            AddDestination(terrain, "TownDestination", "마을 시작점", 1, start, new Vector2(0, 22), font);
            var field = new Vector2(Mathf.Max(end.x, crossing + layout.TransitionMargin) + author.map.tileSize * .5f, end.y);
            if (author.map.CanTraverse(end, field))
                AddDestination(terrain, "FieldDestination", "첫 사냥터 진입", 2, field, new Vector2(10, 16), font);
        }

        private void AddHuntingDestinations(RectTransform terrain, MapAuthoring author, WorldLayout layout, TMP_FontAsset font)
        {
            if (author.map == null || author.huntingDestinations == null) return;
            for (int i = 0; i < author.huntingDestinations.Length && i + 1 < layout.Zones.Length; i++)
            {
                Vector2 point = author.huntingDestinations[i];
                var zone = layout.Zones[i + 1];
                if (!zone.ExclusiveBounds.Contains(point - zone.ZoneOrigin) || !author.map.IsWalkable(point.x, point.y)) continue;
                var zoneContent = content?.GetHuntingZone(zone.Id);
                string label = zone.Id == 2 ? "능선 너머 사냥터" : (zoneContent?.label ?? $"구역 {zone.Id}") + " 입구";
                float offsetX = zone.Column == layout.Columns - 1 ? -134 : 10;
                AddDestination(terrain, $"HuntingDestination{zone.Id}", label, zone.Id, point, new Vector2(offsetX, 18), font);
            }
        }

        private void AddWallFootprint(RectTransform terrain, Transform module)
        {
            if (module == null || !module.gameObject.activeInHierarchy) return;
            var renderers = module.GetComponentsInChildren<MeshRenderer>();
            if (renderers.Length == 0) return;
            Bounds footprint = renderers[0].bounds;
            for (int i = 1; i < renderers.Length; i++) footprint.Encapsulate(renderers[i].bounds);
            var wall = Surface(terrain, "WallFootprint", new Color32(200, 205, 207, 255)).rectTransform;
            wall.anchorMin = MapPoint(new Vector2(footprint.min.x, footprint.min.z));
            wall.anchorMax = MapPoint(new Vector2(footprint.max.x, footprint.max.z));
            wall.offsetMin = wall.offsetMax = Vector2.zero;
        }

        private void AddDestination(RectTransform terrain, string name, string label, int zone, Vector2 position,
            Vector2 offset, TMP_FontAsset font)
        {
            Vector2 point = MapPoint(position);
            var badge = RouteLabel(terrain, name, label, point, offset, font);
            badge.color = new Color32(23, 55, 63, 255);
            var pin = Surface(terrain, name + "Pin", new Color32(139, 229, 255, 255));
            Anchor(pin.rectTransform, point, new Vector2(.5f, .5f), Vector2.zero, new Vector2(13, 13));
            WireButton(badge);
            WireButton(pin);

            void WireButton(UnityEngine.UI.Image face)
            {
                face.raycastTarget = true;
                var button = face.gameObject.AddComponent<UnityEngine.UI.Button>();
                button.targetGraphic = face;
                button.onClick.AddListener(() =>
                {
                    destinationName = destinationName == label ? null : label;
                    destination = position;
                    destinationZone = zone;
                    UpdateRoute();
                });
            }
        }

        private static UnityEngine.UI.Image RouteLabel(RectTransform terrain, string name, string value, Vector2 position,
            Vector2 offset, TMP_FontAsset font)
        {
            var badge = Surface(terrain, name, new Color32(18, 25, 25, 225));
            Anchor(badge.rectTransform, position, new Vector2(0, .5f), offset, new Vector2(124, 24));
            var text = Label(badge.transform, "Label", value, 15, White, TextAlignmentOptions.Center, font);
            text.enableAutoSizing = true;
            text.fontSizeMin = 11;
            text.fontSizeMax = 15;
            Stretch(text.rectTransform, Vector2.zero, Vector2.zero);
            return badge;
        }

        private void UpdateRoute()
        {
            route.Clear();
            bool found = destinationName != null && navigation.TryFindWalkingRoute(currentPosition, destination, route);
            lastRouteOrigin = currentPosition;
            nextRouteSearch = Time.unscaledTime + .6f;
            refreshField = true;
            routeGraphic.SetRoute(route, bounds);
            routeStatus.text = destinationName == null ? "목표 지점을 클릭해 길 보기" :
                found ? "안내 경로 · " + destinationName : "갈 수 있는 길을 찾지 못했습니다";
        }

        internal void Show(int zoneId, Vector2 position)
        {
            Refresh(zoneId, position);
            if (destinationName != null) UpdateRoute();
            SuspendNavigation(false);
            root.SetAsLastSibling();
            root.gameObject.SetActive(true);
        }

        internal void Refresh(int zoneId, Vector2 position)
        {
            currentPosition = position;
            marker.anchorMin = marker.anchorMax = MapPoint(position);
            marker.anchoredPosition = Vector2.zero;
            if (displayedZone == zoneId) return;
            if (zones.TryGetValue(displayedZone, out var previous)) previous.color = Color.clear;
            if (zones.TryGetValue(zoneId, out var current)) current.color = new Color32(98, 234, 164, 47);
            displayedZone = zoneId;
            var data = content == null ? null : content.GetHuntingZone(zoneId);
            location.text = "현재 위치 · " + (data == null ? "구역 " + zoneId : data.label);
        }

        internal void Hide()
        {
            if (root != null) root.gameObject.SetActive(false);
        }

        internal void UpdateField(int zoneId, Vector2 position, bool canGuide)
        {
            if (IsOpen || destinationName == null || !canGuide)
            {
                SuspendNavigation(false);
                return;
            }
            currentPosition = position;
            // Check arrival before throttling updates, including the final short step.
            if (zoneId == destinationZone && (position - destination).sqrMagnitude <= 1f &&
                navigation.CanTraverse(position, destination))
            {
                SuspendNavigation(true);
                return;
            }
            if (!refreshField && (position - lastRouteOrigin).sqrMagnitude < .75f * .75f) return;

            if (route.Count > 1 && navigation.CanTraverse(position, route[1]))
            {
                // Keep a valid suffix so walking beside a wall does not rerun the grid search every tick.
                int next = 1;
                while (next + 1 < route.Count && navigation.CanTraverse(position, route[next + 1])) next++;
                if (next > 1) route.RemoveRange(1, next - 1);
                route[0] = position;
                lastRouteOrigin = position;
            }
            else if (Time.unscaledTime >= nextRouteSearch)
            {
                UpdateRoute();
            }
            else
            {
                fieldGuide?.Hide();
                fieldBadge.SetActive(false);
                return;
            }

            refreshField = false;
            float distance = 0;
            for (int i = 1; i < route.Count; i++) distance += Vector2.Distance(route[i - 1], route[i]);
            fieldStatus.text = route.Count < 2 ? "갈 수 있는 길을 찾지 못했습니다" :
                destinationName + "  ·  " + Mathf.CeilToInt(distance) + " m";
            fieldBadge.SetActive(true);
            if (route.Count < 2)
            {
                fieldGuide?.Hide();
                return;
            }

            if (fieldGuide == null && mapAuthor.ground != null)
            {
                // The project's GraphicsSettings includes URP Unlit in the standalone player.
                var shader = Shader.Find("Universal Render Pipeline/Unlit");
                if (shader != null)
                    fieldGuide = new WorldRouteGuide(mapAuthor.transform.parent,
                        MapAuthoring.WorldBounds(mapAuthor.ground).max.y + .16f, shader);
            }
            fieldGuide?.Show(route, navigation);
        }

        internal void SuspendNavigation(bool clearTarget)
        {
            fieldGuide?.Hide();
            if (fieldBadge != null && fieldBadge.activeSelf) fieldBadge.SetActive(false);
            refreshField = true;
            if (!clearTarget) return;
            destinationName = null;
            route.Clear();
            nextRouteSearch = 0;
            routeGraphic.SetRoute(route, bounds);
            routeStatus.text = "목표 지점을 클릭해 길 보기";
        }

        internal void Destroy()
        {
            if (!Application.isPlaying) return;
            if (root != null) Object.Destroy(root.gameObject);
            if (fieldBadge != null) Object.Destroy(fieldBadge);
            fieldGuide?.Destroy();
        }

        private static RectTransform CreateRect(Transform parent, string name)
        {
            var rect = (RectTransform)new GameObject(name, typeof(RectTransform)).transform;
            rect.SetParent(parent, false);
            return rect;
        }

        private static UnityEngine.UI.Image Surface(Transform parent, string name, Color color)
        {
            var image = CreateRect(parent, name).gameObject.AddComponent<UnityEngine.UI.Image>();
            image.color = color;
            image.raycastTarget = false;
            return image;
        }

        private static TMP_Text Label(Transform parent, string name, string value, float size, Color color,
            TextAlignmentOptions alignment, TMP_FontAsset font)
        {
            var text = CreateRect(parent, name).gameObject.AddComponent<TextMeshProUGUI>();
            text.font = font != null ? font : TMP_Settings.defaultFontAsset;
            text.text = value;
            text.fontSize = size;
            text.color = color;
            text.alignment = alignment;
            text.richText = false;
            text.raycastTarget = false;
            return text;
        }

        private static void Stretch(RectTransform rect, Vector2 min, Vector2 max)
        {
            rect.anchorMin = Vector2.zero;
            rect.anchorMax = Vector2.one;
            rect.offsetMin = min;
            rect.offsetMax = max;
        }

        private static void Anchor(RectTransform rect, Vector2 anchor, Vector2 pivot, Vector2 position, Vector2 size)
        {
            rect.anchorMin = rect.anchorMax = anchor;
            rect.pivot = pivot;
            rect.anchoredPosition = position;
            rect.sizeDelta = size;
        }
    }
}
