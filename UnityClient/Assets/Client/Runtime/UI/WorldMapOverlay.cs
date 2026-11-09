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
        private readonly Dictionary<int, UnityEngine.UI.Image> zones = new Dictionary<int, UnityEngine.UI.Image>();
        private readonly ServerContent content;
        private int displayedZone = -1;

        internal bool IsOpen => root != null && root.gameObject.activeSelf;

        internal WorldMapOverlay(Transform canvas, Texture2D atlas, Rect bounds, MapAuthoring author,
            ServerContent content, TMP_FontAsset font, System.Action close)
        {
            this.bounds = bounds;
            this.content = content;
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

            var north = Label(imageRect, "North", "N ↑", 19, Gold, TextAlignmentOptions.Center, font);
            Anchor(north.rectTransform, Vector2.one, Vector2.one, new Vector2(-8, -6), new Vector2(50, 26));
            marker = Surface(imageRect, "CurrentPosition", White).rectTransform;
            marker.sizeDelta = new Vector2(14, 14);
            marker.localRotation = Quaternion.Euler(0, 0, 45);
            Stretch(Surface(marker, "Pin", new Color32(48, 227, 108, 255)).rectTransform,
                new Vector2(2, 2), new Vector2(-2, -2));
            location = Label(root, "Location", "", 19, White, TextAlignmentOptions.MidlineLeft, font);
            Anchor(location.rectTransform, Vector2.zero, Vector2.zero, new Vector2(34, 16), new Vector2(410, 32));
            root.gameObject.SetActive(false);
        }

        private Vector2 MapPoint(Vector2 position) => new Vector2(
            Mathf.InverseLerp(bounds.xMin, bounds.xMax, position.x),
            Mathf.InverseLerp(bounds.yMin, bounds.yMax, position.y));

        internal void Show(int zoneId, Vector2 position)
        {
            Refresh(zoneId, position);
            root.SetAsLastSibling();
            root.gameObject.SetActive(true);
        }

        internal void Refresh(int zoneId, Vector2 position)
        {
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

        internal void Destroy()
        {
            if (root != null && Application.isPlaying) Object.Destroy(root.gameObject);
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
