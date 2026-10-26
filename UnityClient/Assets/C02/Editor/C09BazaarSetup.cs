using C01;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class C09BazaarSetup
{
    public static void Apply()
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before applying C09 bazaar UI.");
        if (EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Open the C02 scene first.");

        var canvas = Object.FindFirstObjectByType<Canvas>();
        var login = Object.FindFirstObjectByType<C01LoginScreen>();
        var world = Object.FindFirstObjectByType<C02WorldScreen>();
        var combat = Object.FindFirstObjectByType<C05CombatScreen>();
        if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || login == null || world == null || combat == null ||
            canvas.GetComponent<GraphicRaycaster>() == null)
            throw new System.InvalidOperationException("Expected the C02 overlay Canvas, GraphicRaycaster, login, world, and combat screens.");

        var market = Button("C09MarketButton", canvas.transform, "MARKET", new Color(.35f, .25f, .12f, .98f));
        var marketRect = market.GetComponent<RectTransform>();
        marketRect.anchorMin = marketRect.anchorMax = marketRect.pivot = new Vector2(1, 1);
        marketRect.anchoredPosition = new Vector2(-150, -24);
        marketRect.sizeDelta = new Vector2(118, 42);
        market.GetComponentInChildren<TMP_Text>(true).fontSize = 13;
        market.GetComponentInChildren<TMP_Text>(true).color = new Color(.98f, .88f, .64f);

        var panel = FindOrCreate("C09BazaarPanel", canvas.transform, typeof(RectTransform), typeof(Image));
        var panelRect = panel.GetComponent<RectTransform>();
        panelRect.anchorMin = panelRect.anchorMax = panelRect.pivot = new Vector2(1, 0);
        panelRect.anchoredPosition = new Vector2(-24, 22);
        panelRect.sizeDelta = new Vector2(500, 590);
        panel.GetComponent<Image>().color = new Color(.51f, .36f, .16f, .99f);
        panel.GetComponent<Image>().raycastTarget = true;

        var paper = FindOrCreate("BazaarParchment", panel.transform, typeof(RectTransform), typeof(Image));
        Stretch(paper.GetComponent<RectTransform>(), 4);
        paper.GetComponent<Image>().color = new Color(.83f, .75f, .56f, 1);
        paper.GetComponent<Image>().raycastTarget = true;

        var title = Text("BazaarTitle", paper.transform, "BAZAAR  /  LISTINGS", 19, new Color(.22f, .17f, .10f));
        title.alignment = TextAlignmentOptions.MidlineLeft;
        SetTop(title.rectTransform, 15, 30);
        var close = Button("BazaarCloseButton", paper.transform, "CLOSE", new Color(.39f, .27f, .14f, 1));
        AnchorTopRight(close.GetComponent<RectTransform>(), 12, 84, 32);

        var typeButtons = new Button[3];
        string[] names = { "POTIONS", "WEAPONS / SHIELDS", "ARMOR" };
        for (int i = 0; i < typeButtons.Length; i++)
        {
            typeButtons[i] = Button("BazaarType" + (i + 1), paper.transform, names[i], new Color(.42f, .30f, .16f, 1));
            var rect = typeButtons[i].GetComponent<RectTransform>();
            rect.anchorMin = new Vector2(0, 1);
            rect.anchorMax = new Vector2(0, 1);
            rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(15 + i * 154, -98);
            rect.sizeDelta = new Vector2(142, 34);
            typeButtons[i].GetComponentInChildren<TMP_Text>(true).fontSize = 10;
        }

        var searchListings = Button("BazaarSearchListingsButton", paper.transform, "SALES", new Color(.42f, .30f, .16f, 1));
        AnchorTopLeft(searchListings.GetComponent<RectTransform>(), 15, 58, 226, 34);
        var myListings = Button("BazaarMyListingsButton", paper.transform, "MY LISTINGS", new Color(.42f, .30f, .16f, 1));
        AnchorTopLeft(myListings.GetComponent<RectTransform>(), 249, 58, 226, 34);

        Header("BazaarItemHeader", paper.transform, "ITEM", .14f, .48f, TextAlignmentOptions.MidlineLeft);
        Header("BazaarQuantityHeader", paper.transform, "QTY", .41f, .50f, TextAlignmentOptions.Center);
        Header("BazaarPriceHeader", paper.transform, "PRICE", .56f, .77f, TextAlignmentOptions.MidlineRight);
        Header("BazaarStateHeader", paper.transform, "STATE", .78f, .99f, TextAlignmentOptions.Center);

        var scrollObject = FindOrCreate("BazaarScroll", paper.transform, typeof(RectTransform), typeof(ScrollRect));
        Stretch(scrollObject.GetComponent<RectTransform>(), new RectOffset(14, 14, 158, 232));
        var viewport = FindOrCreate("BazaarViewport", scrollObject.transform, typeof(RectTransform), typeof(Image), typeof(Mask));
        Stretch(viewport.GetComponent<RectTransform>(), 0);
        viewport.GetComponent<Image>().color = new Color(.91f, .85f, .70f, .84f);
        viewport.GetComponent<Image>().raycastTarget = true;
        viewport.GetComponent<Mask>().showMaskGraphic = false;
        var rowsObject = FindOrCreate("BazaarRows", viewport.transform, typeof(RectTransform));
        var oldRowsText = rowsObject.GetComponent<TextMeshProUGUI>();
        if (oldRowsText != null) Undo.DestroyObjectImmediate(oldRowsText);
        var rowsFitter = rowsObject.GetComponent<ContentSizeFitter>() ?? Undo.AddComponent<ContentSizeFitter>(rowsObject);
        var rowsLayout = rowsObject.GetComponent<VerticalLayoutGroup>() ?? Undo.AddComponent<VerticalLayoutGroup>(rowsObject);
        var rowsRect = rowsObject.GetComponent<RectTransform>();
        rowsRect.anchorMin = new Vector2(0, 1);
        rowsRect.anchorMax = new Vector2(1, 1);
        rowsRect.pivot = new Vector2(.5f, 1);
        rowsRect.anchoredPosition = Vector2.zero;
        rowsRect.sizeDelta = new Vector2(0, 0);
        rowsLayout.padding = new RectOffset(6, 6, 5, 5);
        rowsLayout.spacing = 4;
        rowsLayout.childControlWidth = true;
        rowsLayout.childControlHeight = true;
        rowsLayout.childForceExpandWidth = true;
        rowsLayout.childForceExpandHeight = false;
        rowsFitter.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        rowsFitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        var scroll = scrollObject.GetComponent<ScrollRect>();
        scroll.viewport = viewport.GetComponent<RectTransform>();
        scroll.content = rowsRect;
        scroll.horizontal = false;
        scroll.vertical = true;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 28;

        var empty = Text("BazaarEmptyState", viewport.transform, "No listings on this page.", 14, new Color(.38f, .31f, .20f));
        empty.alignment = TextAlignmentOptions.Center;
        Stretch(empty.rectTransform, 18);
        empty.gameObject.SetActive(false);

        var detailPanel = FindOrCreate("BazaarSelectedDetailPanel", paper.transform, typeof(RectTransform), typeof(Image));
        AnchorBottomStretch(detailPanel.GetComponent<RectTransform>(), 132, 14, 88);
        detailPanel.GetComponent<Image>().color = new Color(.90f, .84f, .70f, 1);
        var detailIcon = FindOrCreate("DetailItemIcon", detailPanel.transform, typeof(RectTransform), typeof(Image)).GetComponent<Image>();
        detailIcon.preserveAspect = true;
        detailIcon.raycastTarget = false;
        AnchorTopLeft(detailIcon.rectTransform, 8, 8, 48, 48);
        detailIcon.enabled = false;
        var detailName = Text("DetailItemName", detailPanel.transform, "Select an item", 16, new Color(.18f, .16f, .13f));
        AnchorTopLeft(detailName.rectTransform, 66, 8, 190, 24);
        var detailQuantity = Text("DetailItemQuantity", detailPanel.transform, "", 14, new Color(.36f, .34f, .30f));
        AnchorTopLeft(detailQuantity.rectTransform, 66, 32, 190, 22);
        var detail = Text("BazaarSelectedDetail", detailPanel.transform, "Select a listing to see its details.", 14,
            new Color(.28f, .22f, .14f));
        detail.alignment = TextAlignmentOptions.MidlineLeft;
        detail.rectTransform.anchorMin = new Vector2(0, 0);
        detail.rectTransform.anchorMax = new Vector2(1, 0);
        detail.rectTransform.pivot = new Vector2(.5f, 0);
        detail.rectTransform.anchoredPosition = new Vector2(0, 6);
        detail.rectTransform.sizeDelta = new Vector2(-16, 40);
        detail.enableWordWrapping = true;
        detail.overflowMode = TextOverflowModes.Ellipsis;

        var buy = Button("BazaarBuyButton", paper.transform, "BUY SELECTED", new Color(.39f, .27f, .14f, 1));
        AnchorBottomLeft(buy.GetComponent<RectTransform>(), 14, 226, 32);
        buy.GetComponent<RectTransform>().anchoredPosition = new Vector2(14, 94);
        var cancelListing = Button("BazaarCancelListingButton", paper.transform, "CANCEL SALE", new Color(.39f, .27f, .14f, 1));
        AnchorBottomLeft(cancelListing.GetComponent<RectTransform>(), 14, 226, 32);
        cancelListing.GetComponent<RectTransform>().anchoredPosition = new Vector2(14, 94);
        cancelListing.gameObject.SetActive(false);
        var checkOutbox = Button("BazaarCheckOutboxButton", paper.transform, "RECEIVE ITEMS", new Color(.24f, .36f, .22f, 1));
        AnchorBottomRight(checkOutbox.GetComponent<RectTransform>(), 14, 226, 32);
        checkOutbox.GetComponent<RectTransform>().anchoredPosition = new Vector2(-14, 94);

        var status = Text("BazaarStatus", paper.transform, string.Empty, 11, new Color(.30f, .22f, .12f));
        status.alignment = TextAlignmentOptions.MidlineLeft;
        status.rectTransform.anchorMin = new Vector2(0, 0);
        status.rectTransform.anchorMax = new Vector2(1, 0);
        status.rectTransform.pivot = new Vector2(.5f, 0);
        status.rectTransform.anchoredPosition = new Vector2(0, 50);
        status.rectTransform.sizeDelta = new Vector2(-30, 40);

        var search = Button("BazaarSearchButton", paper.transform, "SEARCH / REFRESH", new Color(.39f, .27f, .14f, 1));
        AnchorBottomLeft(search.GetComponent<RectTransform>(), 14, 128, 34);
        var claim = Button("BazaarClaimButton", paper.transform, "COLLECT PROCEEDS", new Color(.24f, .36f, .22f, 1));
        AnchorBottomRight(claim.GetComponent<RectTransform>(), 14, 190, 34);
        claim.gameObject.SetActive(false);
        var page = Text("BazaarPage", paper.transform, "Page 1", 12, new Color(.27f, .20f, .12f));
        page.alignment = TextAlignmentOptions.Center;
        page.rectTransform.anchorMin = new Vector2(1, 0);
        page.rectTransform.anchorMax = page.rectTransform.pivot = new Vector2(1, 0);
        page.rectTransform.anchoredPosition = new Vector2(-150, 18);
        page.rectTransform.sizeDelta = new Vector2(60, 26);
        var previous = Button("BazaarPreviousPageButton", paper.transform, "PREV", new Color(.44f, .32f, .17f, 1));
        AnchorBottomRight(previous.GetComponent<RectTransform>(), 82, 60, 32);
        var next = Button("BazaarNextPageButton", paper.transform, "NEXT", new Color(.44f, .32f, .17f, 1));
        AnchorBottomRight(next.GetComponent<RectTransform>(), 14, 60, 32);

        var screen = canvas.GetComponent<C09BazaarScreen>() ?? Undo.AddComponent<C09BazaarScreen>(canvas.gameObject);
        var serialized = new SerializedObject(screen);
        serialized.FindProperty("loginScreen").objectReferenceValue = login;
        serialized.FindProperty("worldScreen").objectReferenceValue = world;
        serialized.FindProperty("bazaarPanel").objectReferenceValue = panel;
        serialized.FindProperty("marketButton").objectReferenceValue = market;
        serialized.FindProperty("searchListingsButton").objectReferenceValue = searchListings;
        serialized.FindProperty("myListingsButton").objectReferenceValue = myListings;
        serialized.FindProperty("itemTypeButtons").arraySize = typeButtons.Length;
        for (int i = 0; i < typeButtons.Length; i++) serialized.FindProperty("itemTypeButtons").GetArrayElementAtIndex(i).objectReferenceValue = typeButtons[i];
        serialized.FindProperty("searchButton").objectReferenceValue = search;
        serialized.FindProperty("claimButton").objectReferenceValue = claim;
        serialized.FindProperty("buyButton").objectReferenceValue = buy;
        serialized.FindProperty("cancelListingButton").objectReferenceValue = cancelListing;
        serialized.FindProperty("checkOutboxButton").objectReferenceValue = checkOutbox;
        serialized.FindProperty("previousPageButton").objectReferenceValue = previous;
        serialized.FindProperty("nextPageButton").objectReferenceValue = next;
        serialized.FindProperty("closeButton").objectReferenceValue = close;
        serialized.FindProperty("pageText").objectReferenceValue = page;
        serialized.FindProperty("listingRows").objectReferenceValue = rowsRect;
        serialized.FindProperty("emptyStateText").objectReferenceValue = empty;
        serialized.FindProperty("selectedDetailText").objectReferenceValue = detail;
        serialized.FindProperty("selectedItemIcon").objectReferenceValue = detailIcon;
        serialized.FindProperty("selectedItemNameText").objectReferenceValue = detailName;
        serialized.FindProperty("selectedItemQuantityText").objectReferenceValue = detailQuantity;
        serialized.FindProperty("inventoryScreen").objectReferenceValue = canvas.GetComponent<C07InventoryScreen>();
        serialized.FindProperty("statusText").objectReferenceValue = status;
        var sell = Button("BazaarSellItemsButton", paper.transform, "SELL ITEMS", new Color(.14f,.20f,.24f,1));
        AnchorTopLeft(sell.GetComponent<RectTransform>(), 487, 58, 260, 34);
        var form = FindOrCreate("SaleForm", paper.transform, typeof(RectTransform));
        var formRect = form.GetComponent<RectTransform>();
        formRect.anchorMin = formRect.anchorMax = formRect.pivot = new Vector2(1,1);
        formRect.anchoredPosition = new Vector2(-14,-276);
        formRect.sizeDelta = new Vector2(270,192);
        var qtyLabel = Text("QuantityLabel", form.transform, "QUANTITY", 14, Color.white);
        AnchorTopLeft(qtyLabel.rectTransform,0,0,82,20);
        var priceLabel = Text("PriceLabel", form.transform, "TOTAL PRICE · DIAMONDS", 13, new Color(.55f,.83f,.94f));
        AnchorTopLeft(priceLabel.rectTransform,88,0,182,20);
        var quantity = SaleInput("Quantity",form.transform,"1",0,82);
        var price = SaleInput("TotalDiamondPrice",form.transform,"100",88,182);
        Text("StackTotalLabel", form.transform, "STACK TOTAL", 13, new Color(.80f,.84f,.87f));
        CreateCurrencyMark("StackDiamondMark", form.transform, false, 132, 80, false);
        var stackValue = Text("StackTotalValue", form.transform, "--", 15, new Color(.70f,.86f,.93f));
        AnchorTopLeft(stackValue.rectTransform, 152, 68, 118, 24);
        stackValue.alignment = TextAlignmentOptions.MidlineRight;
        var feeLabel = Text("GoldFeeLabel", form.transform, "GOLD FEE", 13, new Color(.80f,.84f,.87f));
        AnchorTopLeft(feeLabel.rectTransform, 0, 68, 118, 24);
        CreateCurrencyMark("GoldFeeMark", form.transform, true, 132, 80, false);
        var feeValue = Text("GoldFeeValue", form.transform, "--", 15, new Color(.94f,.78f,.45f));
        AnchorTopLeft(feeValue.rectTransform, 152, 68, 118, 24);
        feeValue.alignment = TextAlignmentOptions.MidlineRight;
        var afterLabel = Text("GoldAfterLabel", form.transform, "GOLD AFTER", 13, new Color(.80f,.84f,.87f));
        AnchorTopLeft(afterLabel.rectTransform, 0, 128, 118, 24);
        CreateCurrencyMark("GoldAfterMark", form.transform, true, 132, 140, false);
        var afterValue = Text("GoldAfterValue", form.transform, "--", 15, new Color(.94f,.78f,.45f));
        AnchorTopLeft(afterValue.rectTransform, 152, 128, 118, 24);
        afterValue.alignment = TextAlignmentOptions.MidlineRight;
        var summary = Text("FeeSummary",form.transform,"Select an item and enter its total price.",13,new Color(.78f,.82f,.85f));
        AnchorTopLeft(summary.rectTransform,0,100,270,38);
        summary.alignment = TextAlignmentOptions.MidlineLeft;
        summary.enableWordWrapping = true;
        summary.overflowMode = TextOverflowModes.Ellipsis;
        var register = Button("RegisterSaleButton",form.transform,"REVIEW SALE",new Color(.16f,.30f,.33f,1));
        AnchorTopLeft(register.GetComponent<RectTransform>(),0,154,270,34);
        form.SetActive(false);
        var wallet = FindOrCreate("WalletHud",canvas.transform,typeof(RectTransform),typeof(Image));
        var walletRect=wallet.GetComponent<RectTransform>();
        walletRect.anchorMin=walletRect.anchorMax=walletRect.pivot=new Vector2(1,1);
        walletRect.anchoredPosition=new Vector2(-24,-76);walletRect.sizeDelta=new Vector2(430,32);
        wallet.GetComponent<Image>().color=new Color(.035f,.05f,.065f,.88f);
        wallet.GetComponent<Image>().raycastTarget=false;
        CreateCurrencyMark("GoldMark", wallet.transform, true, 20, 16, true);
        CreateCurrencyMark("DiamondMark", wallet.transform, false, 235, 16, true);
        var gold=Text("Gold",wallet.transform,"--",16,new Color(.94f,.76f,.39f));
        var diamond=Text("Diamonds",wallet.transform,"--",16,new Color(.62f,.82f,.88f));
        gold.rectTransform.anchorMin=Vector2.zero;gold.rectTransform.anchorMax=new Vector2(.5f,1);
        gold.rectTransform.offsetMin=new Vector2(34,0);gold.rectTransform.offsetMax=new Vector2(-8,0);
        diamond.rectTransform.anchorMin=new Vector2(.5f,0);diamond.rectTransform.anchorMax=Vector2.one;
        diamond.rectTransform.offsetMin=new Vector2(34,0);diamond.rectTransform.offsetMax=new Vector2(-10,0);
        gold.alignment=diamond.alignment=TextAlignmentOptions.MidlineLeft;
        gold.enableAutoSizing=diamond.enableAutoSizing=true;
        gold.fontSizeMin=diamond.fontSizeMin=12;gold.fontSizeMax=diamond.fontSizeMax=16;
        wallet.SetActive(false);
        serialized.FindProperty("sellItemsButton").objectReferenceValue=sell;
        serialized.FindProperty("registerButton").objectReferenceValue=register;
        serialized.FindProperty("saleForm").objectReferenceValue=form;
        serialized.FindProperty("quantityInput").objectReferenceValue=quantity;
        serialized.FindProperty("priceInput").objectReferenceValue=price;
        serialized.FindProperty("saleSummaryText").objectReferenceValue=summary;
        serialized.FindProperty("saleStackValueText").objectReferenceValue=stackValue;
        serialized.FindProperty("saleFeeValueText").objectReferenceValue=feeValue;
        serialized.FindProperty("saleGoldAfterValueText").objectReferenceValue=afterValue;
        serialized.FindProperty("walletHud").objectReferenceValue=wallet;
        serialized.FindProperty("walletGoldText").objectReferenceValue=gold;
        serialized.FindProperty("walletDiamondText").objectReferenceValue=diamond;
        serialized.FindProperty("diamondCurrencyIcon").objectReferenceValue =
            AssetDatabase.LoadAssetAtPath<Sprite>("Assets/C02/Art/Currency/diamond-currency-icon-v1.png");
        serialized.ApplyModifiedPropertiesWithoutUndo();

        var worldSerialized = new SerializedObject(world);
        worldSerialized.FindProperty("bazaarScreen").objectReferenceValue = screen;
        worldSerialized.ApplyModifiedPropertiesWithoutUndo();
        var combatSerialized = new SerializedObject(combat);
        combatSerialized.FindProperty("bazaarScreen").objectReferenceValue = screen;
        combatSerialized.ApplyModifiedPropertiesWithoutUndo();
        ApplyCleanStyle();
        panel.SetActive(false);
        market.gameObject.SetActive(false);
        EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(SceneManager.GetActiveScene());
    }

    private static void CreateCurrencyMark(string name, Transform parent, bool gold, float x, float y, bool walletHud)
    {
        var image = FindOrCreate(name, parent, typeof(RectTransform), typeof(UnityEngine.UI.Image)).GetComponent<UnityEngine.UI.Image>();
        var icon = AssetDatabase.LoadAssetAtPath<Sprite>(gold
            ? "Assets/C02/Art/Currency/gold-currency-icon-v1.png"
            : "Assets/C02/Art/Currency/diamond-currency-icon-v1.png");
        image.sprite = icon;
        image.preserveAspect = true;
        image.color = icon != null ? Color.white : gold ? new Color(.91f, .70f, .31f) : new Color(.47f, .79f, .89f);
        image.raycastTarget = false;
        var rect = image.rectTransform;
        if (walletHud)
        {
            rect.anchorMin = rect.anchorMax = new Vector2(0, .5f);
            rect.anchoredPosition = new Vector2(x, 0);
        }
        else
        {
            rect.anchorMin = rect.anchorMax = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(x, -y);
        }
        rect.sizeDelta = icon != null ? new Vector2(16, 16) : gold ? new Vector2(14, 8) : new Vector2(10, 10);
        rect.localRotation = icon != null || gold ? Quaternion.identity : Quaternion.Euler(0, 0, 45);
    }

    private static TMP_InputField SaleInput(string name,Transform parent,string initial,float x,float width)
    {
        var go=FindOrCreate(name,parent,typeof(RectTransform),typeof(Image),typeof(TMP_InputField));
        AnchorTopLeft(go.GetComponent<RectTransform>(),x,24,width,32);
        go.GetComponent<Image>().color=new Color(.025f,.035f,.045f,1);
        var input=go.GetComponent<TMP_InputField>();
        var viewport=FindOrCreate("Viewport",go.transform,typeof(RectTransform),typeof(RectMask2D));
        Stretch(viewport.GetComponent<RectTransform>(),5);
        var text=Text("Text",viewport.transform,initial,16,Color.white);
        Stretch(text.rectTransform,0);text.alignment=TextAlignmentOptions.MidlineLeft;
        input.targetGraphic=go.GetComponent<Image>();input.textViewport=viewport.GetComponent<RectTransform>();
        input.textComponent=text;input.contentType=TMP_InputField.ContentType.IntegerNumber;
        input.characterLimit=20;input.lineType=TMP_InputField.LineType.SingleLine;
        input.navigation=new Navigation{mode=Navigation.Mode.None};input.SetTextWithoutNotify(initial);
        return input;
    }

    public static void ApplyCleanStyle()
    {
        var canvas=Object.FindFirstObjectByType<Canvas>();
        var panel=canvas!=null?canvas.transform.Find("C09BazaarPanel") as RectTransform:null;
        if(panel==null)return;
        panel.anchorMin=panel.anchorMax=panel.pivot=new Vector2(1,1);
        panel.anchoredPosition=new Vector2(-24,-118);panel.sizeDelta=new Vector2(780,555);
        foreach(var image in panel.GetComponentsInChildren<Image>(true))
        {
            if(image.name=="ForgedFrame"||image.name=="FrameOrnaments"){image.gameObject.SetActive(false);continue;}
            if(image.name.EndsWith("Mark", System.StringComparison.Ordinal) || image.name=="DetailItemIcon")
            {image.raycastTarget=false;continue;}
            image.sprite=null;image.type=Image.Type.Simple;
            image.color=image.GetComponent<TMP_InputField>()!=null?new Color(.24f,.28f,.32f,1):image.GetComponent<Button>()!=null?new Color(.14f,.18f,.22f,1):new Color(.06f,.08f,.10f,.98f);
        }
        foreach(var button in panel.GetComponentsInChildren<Button>(true))
        {button.navigation=new Navigation{mode=Navigation.Mode.None};var colors=ColorBlock.defaultColorBlock;colors.highlightedColor=new Color(1.3f,1.3f,1.3f);button.colors=colors;}
        foreach(var text in panel.GetComponentsInChildren<TMP_Text>(true))
        {text.color=new Color(.88f,.91f,.95f);text.fontSize=Mathf.Max(14,text.fontSize);}
        var paper=panel.Find("BazaarParchment");if(paper==null)return;
        foreach(var text in paper.Find("BazaarSelectedDetailPanel").GetComponentsInChildren<TMP_Text>(true))
            text.color=new Color(.88f,.91f,.95f);
        var title=paper.Find("BazaarTitle").GetComponent<TMP_Text>();title.text="ITEM EXCHANGE";title.fontSize=21;
        AnchorTopLeft(paper.Find("BazaarSearchListingsButton") as RectTransform,15,58,236,34);
        AnchorTopLeft(paper.Find("BazaarMyListingsButton") as RectTransform,251,58,236,34);
        if(paper.Find("BazaarSellItemsButton")!=null)AnchorTopLeft(paper.Find("BazaarSellItemsButton") as RectTransform,487,58,260,34);
        Stretch(paper.Find("BazaarScroll") as RectTransform,new RectOffset(14,310,158,146));
        // Column headers use the same left-hand list width as runtime rows.
        AnchorTopLeft(paper.Find("BazaarItemHeader") as RectTransform,78,132,120,22);
        AnchorTopLeft(paper.Find("BazaarQuantityHeader") as RectTransform,200,132,42,22);
        AnchorTopLeft(paper.Find("BazaarPriceHeader") as RectTransform,245,132,108,22);
        AnchorTopLeft(paper.Find("BazaarStateHeader") as RectTransform,359,132,80,22);
        var detail=paper.Find("BazaarSelectedDetailPanel") as RectTransform;
        detail.anchorMin=detail.anchorMax=detail.pivot=new Vector2(1,1);
        detail.anchoredPosition=new Vector2(-14,-148);detail.sizeDelta=new Vector2(270,120);
        var status=paper.Find("BazaarStatus") as RectTransform;
        status.anchorMin=status.anchorMax=status.pivot=new Vector2(0,0);
        status.anchoredPosition=new Vector2(14,40);status.sizeDelta=new Vector2(450,44);
        var close=paper.Find("BazaarCloseButton") as RectTransform;
        close.anchoredPosition=new Vector2(-12,-12);
        // Keep legacy generated objects so existing serialized references remain valid, but hide redundant totals.
        var saleForm=paper.Find("SaleForm");
        if(saleForm!=null)
        {
            foreach(var name in new[]{"StackTotalLabel","StackDiamondMark","StackTotalValue","GoldAfterLabel","GoldAfterMark","GoldAfterValue"})
            {
                var obsolete=saleForm.Find(name);
                if(obsolete!=null)obsolete.gameObject.SetActive(false);
            }
            saleForm.GetComponent<RectTransform>().sizeDelta=new Vector2(270,192);
            var feeLabel=saleForm.Find("GoldFeeLabel") as RectTransform;
            var feeMark=saleForm.Find("GoldFeeMark") as RectTransform;
            var feeValue=saleForm.Find("GoldFeeValue") as RectTransform;
            var feeSummary=saleForm.Find("FeeSummary") as RectTransform;
            var registerButton=saleForm.Find("RegisterSaleButton") as RectTransform;
            if(feeLabel!=null)AnchorTopLeft(feeLabel,0,68,118,24);
            if(feeMark!=null)AnchorTopLeft(feeMark,132,80,14,8);
            if(feeValue!=null)AnchorTopLeft(feeValue,152,68,118,24);
            if(feeSummary!=null)AnchorTopLeft(feeSummary,0,100,270,38);
            if(registerButton!=null)AnchorTopLeft(registerButton,0,154,270,34);
        }
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
        var text = FindOrCreate(name, parent, typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();
        text.font = TMP_Settings.defaultFontAsset;
        text.text = value;
        text.fontSize = size;
        text.color = color;
        text.richText = false;
        text.raycastTarget = false;
        return text;
    }

    private static Button Button(string name, Transform parent, string label, Color color)
    {
        var button = FindOrCreate(name, parent, typeof(RectTransform), typeof(Image), typeof(Button)).GetComponent<Button>();
        var image = button.GetComponent<Image>();
        image.color = color;
        button.targetGraphic = image;
        var text = Text("Label", button.transform, label, 12, new Color(.98f, .88f, .67f));
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
    private static void AnchorTopLeft(RectTransform rect, float x, float inset, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
        rect.anchoredPosition = new Vector2(x, -inset);
        rect.sizeDelta = new Vector2(width, height);
    }
    private static void AnchorTopRight(RectTransform rect, float inset, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(1, 1);
        rect.anchoredPosition = new Vector2(-inset, -inset);
        rect.sizeDelta = new Vector2(width, height);
    }
    private static void AnchorBottomLeft(RectTransform rect, float bottom, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 0);
        rect.anchoredPosition = new Vector2(14, bottom);
        rect.sizeDelta = new Vector2(width, height);
    }
    private static void AnchorBottomRight(RectTransform rect, float inset, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(1, 0);
        rect.anchoredPosition = new Vector2(-inset, 14);
        rect.sizeDelta = new Vector2(width, height);
    }
    private static void AnchorBottomStretch(RectTransform rect, float bottom, float horizontalInset, float height)
    {
        rect.anchorMin = new Vector2(0, 0);
        rect.anchorMax = new Vector2(1, 0);
        rect.pivot = new Vector2(.5f, 0);
        rect.anchoredPosition = new Vector2(0, bottom);
        rect.sizeDelta = new Vector2(-horizontalInset * 2, height);
    }
    private static void Header(string name, Transform parent, string value, float left, float right, TextAlignmentOptions alignment)
    {
        var text = Text(name, parent, value, 13, new Color(.38f, .29f, .17f));
        text.alignment = alignment;
        var rect = text.rectTransform;
        rect.anchorMin = new Vector2(left, 1);
        rect.anchorMax = new Vector2(right, 1);
        rect.pivot = new Vector2(.5f, 1);
        rect.anchoredPosition = new Vector2(0, -132);
        rect.sizeDelta = new Vector2(0, 22);
    }
}
