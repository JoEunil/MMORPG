using Mmorpg.Client;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

namespace Mmorpg.Client.Editor
{
    public static class InventorySetup
    {
        public static void Apply()
        {
            if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before applying C07 inventory UI.");
            if (EditorSceneManager.GetActiveScene().path != "Assets/Client/Scenes/World.unity")
                throw new System.InvalidOperationException("Open the C02 scene first.");

            var canvas = Object.FindFirstObjectByType<Canvas>();
            var login = Object.FindFirstObjectByType<LoginScreen>();
            var world = Object.FindFirstObjectByType<WorldScreen>();
            var combat = Object.FindFirstObjectByType<CombatScreen>();
            var bazaar = Object.FindFirstObjectByType<MarketScreen>();
            var eventSystems = Object.FindObjectsByType<EventSystem>(FindObjectsSortMode.None);
            var hud = canvas != null ? canvas.transform.Find("WorldHud") : null;
            if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || canvas.GetComponent<GraphicRaycaster>() == null ||
                login == null || world == null || combat == null || bazaar == null || hud == null || eventSystems.Length != 1)
                throw new System.InvalidOperationException("Expected the C02 overlay Canvas, one EventSystem, WorldHud, login, world, combat, and bazaar screens.");

            var bag = Button("C07BagButton", canvas.transform, "BAG", new Color(.14f, .18f, .22f, .98f));
            var bagRect = bag.GetComponent<RectTransform>();
            bagRect.anchorMin = bagRect.anchorMax = bagRect.pivot = new Vector2(1, 1);
            bagRect.anchoredPosition = new Vector2(-24, -24);
            bagRect.sizeDelta = new Vector2(118, 42);
            bag.GetComponentInChildren<TMP_Text>(true).fontSize = 14;
            bag.GetComponentInChildren<TMP_Text>(true).color = new Color(.98f, .89f, .66f);

            var panel = FindOrCreate("C07InventoryPanel", canvas.transform, typeof(RectTransform), typeof(Image));
            var panelRect = panel.GetComponent<RectTransform>();
            panelRect.anchorMin = panelRect.anchorMax = panelRect.pivot = new Vector2(.5f, .5f);
            panelRect.anchoredPosition = Vector2.zero;
            panelRect.sizeDelta = new Vector2(920, 560);
            var frame = panel.GetComponent<Image>();
            frame.color = new Color(.40f, .47f, .52f, .98f);
            frame.raycastTarget = true;

            var inner = FindOrCreate("InventoryParchment", panel.transform, typeof(RectTransform), typeof(Image));
            Stretch(inner.GetComponent<RectTransform>(), 4);
            var paper = inner.GetComponent<Image>();
            paper.color = new Color(.055f, .068f, .078f, .98f);
            paper.raycastTarget = true;

            var title = Text("InventoryTitle", inner.transform, "INVENTORY", 21, new Color(.88f, .91f, .95f));
            title.alignment = TextAlignmentOptions.MidlineLeft;
            SetTop(title.rectTransform, 15, 32);
            var close = Button("InventoryCloseButton", inner.transform, "CLOSE", new Color(.20f, .24f, .28f, 1f));
            var closeRect = close.GetComponent<RectTransform>();
            closeRect.anchorMin = closeRect.anchorMax = closeRect.pivot = new Vector2(1, 1);
            closeRect.anchoredPosition = new Vector2(-12, -12);
            closeRect.sizeDelta = new Vector2(86, 32);
            close.GetComponentInChildren<TMP_Text>(true).fontSize = 13;

            var oldRows = inner.transform.Find("InventoryRows");
            if (oldRows != null) Undo.DestroyObjectImmediate(oldRows.gameObject);

            var scrollObject = FindOrCreate("InventoryScroll", inner.transform, typeof(RectTransform), typeof(ScrollRect));
            var scrollRect = scrollObject.GetComponent<RectTransform>();
            scrollRect.anchorMin = Vector2.zero;
            scrollRect.anchorMax = new Vector2(.64f, 1);
            scrollRect.offsetMin = new Vector2(14, 62);
            scrollRect.offsetMax = new Vector2(-8, -70);
            var viewportObject = FindOrCreate("InventoryViewport", scrollObject.transform, typeof(RectTransform), typeof(Image), typeof(Mask));
            var viewportRect = viewportObject.GetComponent<RectTransform>();
            Stretch(viewportRect, 0);
            viewportObject.GetComponent<Image>().color = new Color(.08f, .10f, .12f, .92f);
            viewportObject.GetComponent<Image>().raycastTarget = true;
            viewportObject.GetComponent<Mask>().showMaskGraphic = false;

            var gridObject = FindOrCreate("InventoryGridContent", viewportObject.transform,
                typeof(RectTransform), typeof(GridLayoutGroup), typeof(ContentSizeFitter));
            var gridRect = gridObject.GetComponent<RectTransform>();
            gridRect.anchorMin = new Vector2(0, 1);
            gridRect.anchorMax = Vector2.one;
            gridRect.pivot = new Vector2(.5f, 1);
            gridRect.anchoredPosition = Vector2.zero;
            gridRect.sizeDelta = Vector2.zero;
            var grid = gridObject.GetComponent<GridLayoutGroup>();
            grid.cellSize = new Vector2(120, 110);
            grid.spacing = new Vector2(8, 8);
            grid.padding = new RectOffset(10, 10, 10, 10);
            grid.startCorner = GridLayoutGroup.Corner.UpperLeft;
            grid.startAxis = GridLayoutGroup.Axis.Horizontal;
            grid.childAlignment = TextAnchor.UpperLeft;
            grid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
            grid.constraintCount = 4;
            var fitter = gridObject.GetComponent<ContentSizeFitter>();
            fitter.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
            fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

            var scroll = scrollObject.GetComponent<ScrollRect>();
            scroll.viewport = viewportRect;
            scroll.content = gridRect;
            scroll.horizontal = false;
            scroll.vertical = true;
            scroll.movementType = ScrollRect.MovementType.Clamped;
            scroll.scrollSensitivity = 28;

            var details = FindOrCreate("InventoryDetails", inner.transform, typeof(RectTransform), typeof(Image));
            var detailsRect = details.GetComponent<RectTransform>();
            detailsRect.anchorMin = new Vector2(.65f, 0);
            detailsRect.anchorMax = Vector2.one;
            detailsRect.offsetMin = new Vector2(8, 62);
            detailsRect.offsetMax = new Vector2(-14, -70);
            var badge = Text("InventoryDetailBadge", details.transform, string.Empty, 28, Color.white);
            var itemName = Text("InventoryDetailName", details.transform, "Select an item", 25, new Color(.94f, .95f, .96f));
            var slot = Text("InventoryDetailSlot", details.transform, string.Empty, 18, new Color(.74f, .79f, .84f));
            var quantity = Text("InventoryDetailQuantity", details.transform, string.Empty, 18, new Color(.74f, .79f, .84f));
            ApplyDetailLayout(details.GetComponent<Image>(), badge, itemName, slot, quantity);
            foreach (string name in new[] { "ForgedFrame", "FrameOrnaments" })
                panel.transform.Find(name)?.gameObject.SetActive(false);

            var status = Text("InventoryStatus", inner.transform, string.Empty, 12, new Color(.82f, .86f, .90f));
            status.alignment = TextAlignmentOptions.MidlineLeft;
            status.enableWordWrapping = true;
            status.rectTransform.anchorMin = status.rectTransform.anchorMax = status.rectTransform.pivot = Vector2.zero;
            status.rectTransform.anchoredPosition = new Vector2(15, 12);
            status.rectTransform.sizeDelta = new Vector2(500, 36);
            status.fontSize = 15;

            var refresh = Button("InventoryRefreshButton", inner.transform, "REFRESH", new Color(.20f, .24f, .28f, 1f));
            var refreshRect = refresh.GetComponent<RectTransform>();
            refreshRect.anchorMin = refreshRect.anchorMax = refreshRect.pivot = new Vector2(1, 0);
            refreshRect.anchoredPosition = new Vector2(-14, 12);
            refreshRect.sizeDelta = new Vector2(112, 34);
            refresh.GetComponentInChildren<TMP_Text>(true).fontSize = 14;

            var screen = canvas.GetComponent<InventoryScreen>() ?? Undo.AddComponent<InventoryScreen>(canvas.gameObject);
            var serialized = new SerializedObject(screen);
            serialized.FindProperty("loginScreen").objectReferenceValue = login;
            serialized.FindProperty("worldScreen").objectReferenceValue = world;
            serialized.FindProperty("bazaarScreen").objectReferenceValue = bazaar;
            serialized.FindProperty("inventoryPanel").objectReferenceValue = panel;
            serialized.FindProperty("bagButton").objectReferenceValue = bag;
            serialized.FindProperty("refreshButton").objectReferenceValue = refresh;
            serialized.FindProperty("closeButton").objectReferenceValue = close;
            serialized.FindProperty("inventoryScroll").objectReferenceValue = scroll;
            serialized.FindProperty("inventoryGridContent").objectReferenceValue = gridRect;
            serialized.FindProperty("detailBadge").objectReferenceValue = badge;
            serialized.FindProperty("detailName").objectReferenceValue = itemName;
            serialized.FindProperty("detailSlot").objectReferenceValue = slot;
            serialized.FindProperty("detailQuantity").objectReferenceValue = quantity;
            serialized.FindProperty("inventoryStatus").objectReferenceValue = status;
            serialized.ApplyModifiedPropertiesWithoutUndo();

            var worldSerialized = new SerializedObject(world);
            worldSerialized.FindProperty("inventoryScreen").objectReferenceValue = screen;
            worldSerialized.ApplyModifiedPropertiesWithoutUndo();
            var combatSerialized = new SerializedObject(combat);
            combatSerialized.FindProperty("inventoryScreen").objectReferenceValue = screen;
            combatSerialized.ApplyModifiedPropertiesWithoutUndo();
            ApplyRestrainedInventoryDetails();
            panel.SetActive(false);
            bag.gameObject.SetActive(false);
            EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
            EditorSceneManager.SaveScene(SceneManager.GetActiveScene());
        }

        [MenuItem("Tools/Client/Inventory/Apply Restrained Inventory Details")]
        public static void ApplyRestrainedInventoryDetails()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode)
                throw new System.InvalidOperationException("Inventory UI authoring requires Edit Mode.");
            var scene = EditorSceneManager.GetActiveScene();
            if (!scene.IsValid() || scene.path != "Assets/Client/Scenes/World.unity")
                throw new System.InvalidOperationException("C07 requires the C02 scene.");

            var screen = Object.FindFirstObjectByType<InventoryScreen>(FindObjectsInactive.Include);
            var panelProperty = screen != null ? new SerializedObject(screen).FindProperty("inventoryPanel") : null;
            var inventoryPanel = panelProperty != null ? panelProperty.objectReferenceValue as GameObject : null;
            if (inventoryPanel == null) throw new System.InvalidOperationException("The existing C07 inventory panel is required.");
            var inner = inventoryPanel.transform.Find("InventoryParchment");
            var details = inner != null ? inner.Find("InventoryDetails") : null;
            var badge = details != null ? details.Find("InventoryDetailBadge")?.GetComponent<TMP_Text>() : null;
            var itemName = details != null ? details.Find("InventoryDetailName")?.GetComponent<TMP_Text>() : null;
            var slot = details != null ? details.Find("InventoryDetailSlot")?.GetComponent<TMP_Text>() : null;
            var quantity = details != null ? details.Find("InventoryDetailQuantity")?.GetComponent<TMP_Text>() : null;
            if (inner == null || details == null || badge == null || itemName == null || slot == null || quantity == null)
                throw new System.InvalidOperationException("The existing C07 detail hierarchy is incomplete.");

            var panelImage = inventoryPanel.GetComponent<Image>();
            var paperImage = inner.GetComponent<Image>();
            var detailsImage = details.GetComponent<Image>();
            if (panelImage == null || paperImage == null || detailsImage == null)
                throw new System.InvalidOperationException("The existing C07 panel surfaces are incomplete.");
            panelImage.sprite = null;
            panelImage.color = new Color32(16, 37, 42, 255);
            var panelEdge = inventoryPanel.GetComponent<UnityEngine.UI.Outline>() ?? inventoryPanel.AddComponent<UnityEngine.UI.Outline>();
            panelEdge.effectColor = new Color32(107, 146, 144, 255);
            panelEdge.effectDistance = new Vector2(.7f, -.7f);
            paperImage.sprite = null;
            paperImage.color = new Color32(16, 37, 42, 255);
            foreach (string name in new[] { "ForgedFrame", "FrameOrnaments" })
                inventoryPanel.transform.Find(name)?.gameObject.SetActive(false);
            foreach (var button in inventoryPanel.GetComponentsInChildren<Button>(true))
            {
                var surface = button.GetComponent<Image>();
                if (surface == null) continue;
                surface.sprite = null;
                surface.color = new Color32(23, 49, 57, 255);
                var edge = button.GetComponent<UnityEngine.UI.Outline>() ?? button.gameObject.AddComponent<UnityEngine.UI.Outline>();
                edge.effectColor = new Color32(107, 146, 144, 180);
                edge.effectDistance = new Vector2(.7f, -.7f);
                var colors = ColorBlock.defaultColorBlock;
                colors.highlightedColor = new Color(1.2f, 1.2f, 1.2f);
                colors.pressedColor = new Color(.8f, .8f, .8f);
                button.colors = colors;
                foreach (string name in new[] { "ForgedFrame", "FrameOrnaments" })
                    button.transform.Find(name)?.gameObject.SetActive(false);
            }
            ApplyDetailLayout(detailsImage, badge, itemName, slot, quantity);
            foreach (var text in inventoryPanel.GetComponentsInChildren<TMP_Text>(true))
                text.color = new Color32(236, 240, 232, 255);
            inner.Find("InventoryTitle").GetComponent<TMP_Text>().color = new Color32(229, 211, 163, 255);
            slot.color = quantity.color = new Color32(175, 193, 192, 255);
            EditorSceneManager.MarkSceneDirty(scene);
        }

        private static void ApplyDetailLayout(Image detailsImage, TMP_Text badge, TMP_Text itemName,
            TMP_Text slot, TMP_Text quantity)
        {
            detailsImage.color = new Color32(25, 56, 59, 255);
            detailsImage.raycastTarget = false;
            badge.alignment = TextAlignmentOptions.Center;
            badge.rectTransform.anchorMin = badge.rectTransform.anchorMax = badge.rectTransform.pivot = new Vector2(.5f, 1);
            badge.rectTransform.anchoredPosition = new Vector2(0, -18);
            badge.rectTransform.sizeDelta = new Vector2(132, 132);
            itemName.fontSize = 25;
            itemName.color = new Color(.94f, .95f, .96f);
            itemName.alignment = TextAlignmentOptions.Center;
            itemName.fontStyle = FontStyles.Bold;
            itemName.enableWordWrapping = true;
            itemName.overflowMode = TextOverflowModes.Ellipsis;
            SetTop(itemName.rectTransform, 160, 58);
            slot.alignment = TextAlignmentOptions.MidlineLeft;
            SetTop(slot.rectTransform, 238, 28);
            quantity.alignment = TextAlignmentOptions.MidlineLeft;
            SetTop(quantity.rectTransform, 274, 28);
        }

        private static GameObject FindOrCreate(string name, Transform parent, params System.Type[] components)
        {
            var existing = parent.Find(name);
            if (existing != null) return existing.gameObject;
            var created = new GameObject(name, components);
            created.transform.SetParent(parent, false);
            return created;
        }

        private static TMP_Text Text(string name, Transform parent, string value, int size, Color color)
        {
            var go = FindOrCreate(name, parent, typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();
            go.font = TMP_Settings.defaultFontAsset;
            go.text = value;
            go.fontSize = size;
            go.color = color;
            go.richText = false;
            go.raycastTarget = false;
            return go;
        }

        private static Button Button(string name, Transform parent, string label, Color color)
        {
            var button = FindOrCreate(name, parent, typeof(RectTransform), typeof(Image), typeof(Button)).GetComponent<Button>();
            var image = button.GetComponent<Image>();
            image.color = color;
            button.targetGraphic = image;
            var text = Text("Label", button.transform, label, 13, Color.white);
            text.alignment = TextAlignmentOptions.Center;
            Stretch(text.rectTransform, 2);
            return button;
        }

        private static void Stretch(RectTransform rect, int margin) => Stretch(rect, new RectOffset(margin, margin, margin, margin));

        private static void Stretch(RectTransform rect, RectOffset padding)
        {
            rect.anchorMin = Vector2.zero;
            rect.anchorMax = Vector2.one;
            rect.offsetMin = new Vector2(padding.left, padding.bottom);
            rect.offsetMax = new Vector2(-padding.right, -padding.top);
        }

        private static void SetTop(RectTransform rect, float inset, float height)
        {
            rect.anchorMin = new Vector2(0, 1);
            rect.anchorMax = new Vector2(1, 1);
            rect.pivot = new Vector2(.5f, 1);
            rect.anchoredPosition = new Vector2(0, -inset);
            rect.sizeDelta = new Vector2(-24, height);
        }
    }
}
