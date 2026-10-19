#if UNITY_EDITOR
using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using TMPro;
using UnityEngine;

namespace C02.Tests
{
    // Passive checks for the mouse/keyboard driven item/market batch, never UI actions.
    public static class C11LiveItemUiTests
    {
        private static C01LoginScreen login;
        private static C02WorldScreen world;
        private static C07InventoryScreen bag;
        private static C09BazaarScreen market;
        private static IViewModelUI model;
        private static string output, run, failure;
        private static int marketResponses, inventoryResponses, ownActions;
        private static Vector2 position;
        private static int hp, mp;
        private static uint internalID;
        private static readonly bool[] MarketModes = { false, true, true, false };

        public static void Begin(string id)
        {
            if (!UnityEditor.EditorApplication.isPlaying || !Regex.IsMatch(id ?? "", "^[A-Za-z0-9-]{1,40}$"))
                throw new InvalidOperationException("Play and a valid run ID are required.");
            if (model != null) throw new InvalidOperationException("An observation is active.");
            login = UnityEngine.Object.FindFirstObjectByType<C01LoginScreen>();
            world = UnityEngine.Object.FindFirstObjectByType<C02WorldScreen>();
            bag = UnityEngine.Object.FindFirstObjectByType<C07InventoryScreen>();
            market = UnityEngine.Object.FindFirstObjectByType<C09BazaarScreen>();
            Require(login != null && login.Model != null && world != null && world.HasOwnFull &&
                world.Stage == C02Stage.InWorld && world.SelectedCharacterID == 5011 &&
                bag != null && market != null && !bag.IsOpen && !market.IsOpen, "Start in the approved character's field with both panels closed.");
            run = id;
            output = PathFor("item-market-" + run + ".txt");
            if (File.Exists(output)) throw new IOException("Earlier observation exists.");
            position = world.ServerPosition; hp = world.OwnFull.hp; mp = world.OwnFull.mp; internalID = world.InternalID;
            marketResponses = inventoryResponses = ownActions = 0; failure = null;
            File.WriteAllText(output, "Physical UI input, passive actual responses. No trading or seeded data.\n");
            model = login.Model;
            model.OnBazaarReceived += MarketObserved;
            model.OnInventoryReceived += InventoryObserved;
            model.OnActionResultReceived += ActionObserved;
            Log("BEGIN " + State());
        }

        private static void MarketObserved(bool success)
        {
            try
            {
                Require(model == login.Model && marketResponses < MarketModes.Length, "Unexpected market response or client.");
                bool mine = Read<bool>(market, "myListings");
                Require(mine == MarketModes[marketResponses++], "Unexpected market tab response order.");
                Require(success && market.IsOpen && !bag.IsOpen, "Market must show a successful actual response with bag closed.");
                var rows = login.Data.GetBazaar();
                var shown = Read<Transform>(market, "listingRows").Cast<Transform>().Where(t => t.gameObject.activeSelf).ToArray();
                Require(rows.Count <= 30 && shown.Length == rows.Count, "Actual row count differs from the screen.");
                foreach (var row in rows)
                {
                    var visual = shown.Single(t => t.name == "Listing-" + row.ListingID.ToString(CultureInfo.InvariantCulture));
                    Require(Text(visual, "Item") == C11ItemPresentation.Name(row.ItemID), "Item name differs from server item ID.");
                    Require(Text(visual, "Quantity") == row.Quantity.ToString(CultureInfo.InvariantCulture), "Quantity differs.");
                    Require(Text(visual, "Price") == row.Price.ToString(CultureInfo.InvariantCulture), "Price differs.");
                    Require(Text(visual, "State") == (row.Status == 0 ? "Trading" : row.Status == 1 ? "Sold" : "Claimed"), "Listing status differs.");
                    if (mine) Require(row.SellerCharacterID == world.SelectedCharacterID, "My-list row has another owner.");
                }
                var empty = Read<TMP_Text>(market, "emptyStateText");
                Require(empty.gameObject.activeSelf == (rows.Count == 0), "Empty result state differs from server data.");
                Unchanged();
                Log("MARKET " + marketResponses + " success=True mine=" + mine + " rows=" + rows.Count + " UI=match " + State());
            }
            catch (Exception e) { Failed(e); }
        }

        private static void InventoryObserved(bool success)
        {
            try
            {
                inventoryResponses++;
                Require(model == login.Model && bag.IsOpen && !market.IsOpen, "Inventory response outside the open bag.");
                var cards = Read<RectTransform>(bag, "inventoryGridContent").Cast<Transform>().Where(t => t.gameObject.activeSelf).ToArray();
                var rows = login.Data.GetInventory().OrderBy(item => item.Slot).ToArray();
                Require(cards.Length == (success ? rows.Length : 0), "Inventory cards differ from the actual result.");
                if (success)
                    for (int i = 0; i < rows.Length; i++)
                    {
                        Require(Text(cards[i], "ItemName") == C11ItemPresentation.Name(rows[i].ItemID), "Inventory item name differs.");
                        Require(Text(cards[i], "ItemQuantity") == "Qty " + rows[i].Quantity.ToString(CultureInfo.InvariantCulture), "Inventory quantity differs.");
                    }
                Require(!string.IsNullOrWhiteSpace(Read<TMP_Text>(bag, "inventoryStatus").text), "Inventory status is missing.");
                Unchanged();
                Log("INVENTORY " + inventoryResponses + " success=" + success + " cards=" + cards.Length +
                    " UI=match status=" + Read<TMP_Text>(bag, "inventoryStatus").text + " " + State());
            }
            catch (Exception e) { Failed(e); }
        }

        private static void ActionObserved(ushort count, ActionResultField[] actions)
        {
            if (actions == null) return;
            for (int i = 0; i < Math.Min(count, actions.Length); i++)
                if (actions[i].casterType == 0 && actions[i].casterId == internalID) ownActions++;
        }

        public static void SelectedListing()
        {
            Require(failure == null && market.IsOpen && login.Data.GetBazaar().Count > 0, failure ?? "No visible listing to inspect.");
            var row = login.Data.GetBazaar()[0];
            string detail = Read<TMP_Text>(market, "selectedDetailText").text;
            Require(detail.Contains(C11ItemPresentation.Name(row.ItemID)) &&
                detail.Contains("Listing #" + row.ListingID.ToString(CultureInfo.InvariantCulture)) &&
                detail.Contains("Seller #" + row.SellerCharacterID.ToString(CultureInfo.InvariantCulture)) &&
                detail.Contains(row.RegisteredAt.ToString(CultureInfo.InvariantCulture)), "Physical first-row selection shows different details.");
            Log("PASS physical listing selection matches actual data.");
        }

        public static string Checkpoint(int expectedMarket, int expectedInventory, string label)
        {
            Require(model != null && model == login.Model && failure == null, failure ?? "No observation.");
            Require(marketResponses == expectedMarket && inventoryResponses == expectedInventory, "Missing or extra actual response.");
            Require(!(bag.IsOpen && market.IsOpen), "Panels overlap.");
            Unchanged();
            if (!Regex.IsMatch(label ?? "", "^[A-Za-z0-9-]{1,40}$")) throw new ArgumentException("Invalid capture label.");
            string path = PathFor("item-market-" + run + "-" + label + ".png");
            if (File.Exists(path)) throw new IOException("Earlier capture exists.");
            Log("CHECKPOINT " + label + " market=" + marketResponses + " inventory=" + inventoryResponses + " ownActions=" + ownActions + " " + State());
            ScreenCapture.CaptureScreenshot(path);
            return path;
        }

        public static void Finish()
        {
            try
            {
                Require(failure == null, failure);
                Require(marketResponses == 4 && inventoryResponses == 2 && ownActions == 0, "The complete response/input batch is required.");
                Require(login.Model == null && !world.IsWorldVisible && !bag.IsOpen && !market.IsOpen, "Normal physical logout is required.");
                Log("PASS complete physical item/market flow; inventory server success/failure recorded separately; no own skill action result.");
            }
            finally { Detach(); }
        }

        public static void Abort(string reason) { try { if (output != null) Log("ABORT " + reason); } finally { Detach(); } }
        private static void Detach()
        {
            if (model != null) { model.OnBazaarReceived -= MarketObserved; model.OnInventoryReceived -= InventoryObserved; model.OnActionResultReceived -= ActionObserved; }
            model = null;
        }
        private static void Unchanged()
        {
            Require(world.ServerPosition == position && world.OwnFull.hp == hp && world.OwnFull.mp == mp && ownActions == 0, "Position, HP/MP, or own skill action changed during UI input.");
        }
        private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
        private static string Text(Transform row, string name) => row.GetComponentsInChildren<TMP_Text>(true).Single(t => t.name == name).text;
        private static T Read<T>(object target, string field) => (T)target.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static string State() => string.Format(CultureInfo.InvariantCulture, "x={0:R} y={1:R} HP={2} MP={3}", world.ServerPosition.x, world.ServerPosition.y, world.OwnFull.hp, world.OwnFull.mp);
        private static string PathFor(string name) => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", name));
        private static void Log(string line) => File.AppendAllText(output, line + Environment.NewLine);
        private static void Failed(Exception e) { failure = failure == null ? e.Message : failure + " | " + e.Message; Log("FAIL " + e.Message); }
    }
}
#endif
