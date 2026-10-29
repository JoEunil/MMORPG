#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using Mmorpg.Client;
using ClientCore;
using NUnit.Framework;
using TMPro;
using UnityEngine;

namespace Mmorpg.Client.Tests
{
    // Passive observer for the real mouse-driven C10 flow; it never calls UI actions or fabricates responses.
    public static class LiveMarketSettlementUiTests
    {
        private static LoginScreen login;
        private static WorldScreen world;
        private static MarketScreen screen;
        private static IViewModelUI model;
        private static string output, runId, observationFailure;
        private static Action<bool> observer;
        private static Vector2 position;
        private static int hp, mp, responses, lastRows;
        private static bool lastSuccess, lastMyList;
        private static readonly bool[] expectedModes = { false, true, true, false };

        public static void Begin(string value, ulong characterID)
        {
            ValidateRunId(value);
            if (model != null) throw new InvalidOperationException("A C10 observation is already active.");
            login = GameObject.Find("C01Client").GetComponent<LoginScreen>();
            world = GameObject.Find("C02Client").GetComponent<WorldScreen>();
            screen = UnityEngine.Object.FindFirstObjectByType<MarketScreen>();
            Assert.That(login.Model != null && login.Data != null && world.Stage == WorldStage.InWorld &&
                world.SelectedCharacterID == characterID && world.HasOwnFull && world.IsWorldVisible && screen != null && !screen.IsOpen, Is.True,
                "Begin requires the requested logged-in character and a confirmed visible field.");

            runId = value;
            output = PathFor("c10-physical-" + runId + ".txt");
            if (File.Exists(output)) throw new IOException("Earlier observation evidence exists.");
            position = world.ServerPosition;
            hp = world.OwnFull.hp;
            mp = world.OwnFull.mp;
            responses = 0;
            lastRows = 0;
            lastSuccess = false;
            lastMyList = false;
            observationFailure = null;
            model = login.Model;
            File.WriteAllText(output, "Physical Windows input; passive observation of actual market responses.\n" +
                "Expected order: search, my listings, my listings refresh, return to search. No trading operation is invoked.\n");
            observer = Observe;
            model.OnBazaarReceived += observer;
            Append("BEGIN character=" + characterID.ToString(CultureInfo.InvariantCulture) + " " + State());
        }

        private static void Observe(bool success)
        {
            try
            {
                if (login.Model != model) throw new InvalidOperationException("The authenticated client changed during observation.");
                if (responses >= expectedModes.Length) throw new InvalidOperationException("An unexpected extra market response arrived.");
                responses++;
                lastSuccess = success;
                lastMyList = Read<object>(screen, "view").ToString() == "MyListings";
                List<BazaarListingView> data = login.Data.GetBazaar();
                string rows = Read<TMP_Text>(screen, "rowsText").text;
                string status = Read<TMP_Text>(screen, "statusText").text;
                lastRows = data.Count;
                bool modeMatches = lastMyList == expectedModes[responses - 1];
                bool uiMatches = rows == BuildRows(data, lastMyList);
                bool statusMatches = status == BuildStatus(data.Count, lastMyList, Read<ushort>(screen, "itemType"));
                Append("RESPONSE " + responses + " success=" + success + " mode=" + (lastMyList ? "my-list" : "search") +
                    " expectedModeMatch=" + modeMatches + " rows=" + data.Count + " uiRowsMatch=" + uiMatches +
                    " uiStatusMatch=" + statusMatches +
                    " status=" + SafeText(status) + " data=" + Describe(data) + " " + State());
                if (!success) throw new InvalidOperationException("The server reported a failed market query; see status text in the observation log.");
                if (!modeMatches) throw new InvalidOperationException("Responses did not follow search → my listings → refresh → search.");
                if (data.Count > 30 || !uiMatches || !statusMatches)
                    throw new InvalidOperationException("Displayed rows or status differ from the actual ViewData snapshot.");
                if (lastMyList)
                    foreach (BazaarListingView row in data)
                        if (row.SellerCharacterID != world.SelectedCharacterID)
                            throw new InvalidOperationException("A my-list row is not owned by the selected character.");
                AssertWorldUnchanged();
            }
            catch (Exception exception)
            {
                observationFailure = observationFailure == null ? exception.Message : observationFailure + " | " + exception.Message;
                try { Append("FAIL OBSERVATION " + exception.Message); } catch { }
            }
        }

        public static string Checkpoint(int expectedResponses, string label)
        {
            EnsureActive();
            ValidateLabel(label);
            Assert.That(observationFailure, Is.Null, observationFailure);
            Assert.That(expectedResponses >= 1 && expectedResponses <= expectedModes.Length && responses == expectedResponses, Is.True,
                "Unexpected or missing real server response at " + label + ".");
            Assert.That(lastSuccess, Is.True, "The latest query must be a successful server response; empty success is valid.");
            Assert.That(lastMyList, Is.EqualTo(expectedModes[expectedResponses - 1]));
            Assert.That(screen.IsOpen && world.Stage == WorldStage.InWorld && world.HasOwnFull, Is.True);
            AssertWorldUnchanged();
            Append("CHECKPOINT " + label + " responses=" + responses + " success=" + lastSuccess +
                " mode=" + (lastMyList ? "my-list" : "search") + " rows=" + lastRows +
                " status=" + SafeText(Read<TMP_Text>(screen, "statusText").text) + " " + State());
            string path = PathFor("c10-physical-" + runId + "-" + label + ".png");
            if (File.Exists(path)) throw new IOException("Earlier capture exists; refusing to overwrite it.");
            ScreenCapture.CaptureScreenshot(path);
            return path;
        }

        public static void CloseCheckpoint()
        {
            EnsureActive();
            Assert.That(observationFailure, Is.Null, observationFailure);
            Assert.That(screen.IsOpen, Is.False, "The physical Close button should hide the market.");
            Assert.That(responses, Is.EqualTo(expectedModes.Length), "Close follows the four expected real queries.");
            Assert.That(Read<TMP_Text>(screen, "rowsText").text, Is.Empty, "Close clears visible rows.");
            AssertWorldUnchanged();
            Append("PASS physical close: panel hidden, rows cleared, position/HP/MP unchanged. " + State());
        }

        public static void Finish()
        {
            try
            {
                Assert.That(observationFailure, Is.Null, observationFailure);
                Assert.That(responses, Is.EqualTo(expectedModes.Length), "All four actual search/my-list responses are required.");
                Assert.That(login.Model == null && !world.IsWorldVisible, Is.True, "Finish requires normal UI logout and hidden world.");
                Append("PASS four successful actual queries (including valid empty results), close, and normal UI logout. " + State());
            }
            finally { Detach(); }
        }

        public static void Abort(string reason)
        {
            try { if (!string.IsNullOrWhiteSpace(output)) Append("ABORT " + SafeText(reason)); }
            finally { Detach(); }
        }

        private static void EnsureActive()
        {
            if (model == null || login == null || world == null || screen == null || login.Model != model)
                throw new InvalidOperationException("Begin a fresh C10 observation before using checkpoints.");
        }

        private static void Detach()
        {
            if (model != null && observer != null) model.OnBazaarReceived -= observer;
            model = null;
            observer = null;
        }

        private static void AssertWorldUnchanged()
        {
            Assert.That(world.ServerPosition, Is.EqualTo(position), "The market read flow must not move the character.");
            Assert.That(world.OwnFull.hp, Is.EqualTo(hp), "HP changed during the read flow.");
            Assert.That(world.OwnFull.mp, Is.EqualTo(mp), "MP changed during the read flow.");
        }

        private static string BuildRows(List<BazaarListingView> rows, bool myList)
        {
            var text = new StringBuilder(rows.Count * 100);
            foreach (BazaarListingView row in rows)
            {
                if (text.Length != 0) text.Append('\n');
                if (myList)
                    text.Append("Listing #").Append(row.ListingID.ToString(CultureInfo.InvariantCulture))
                        .Append(" · ").Append(ListingStatusName(row.Status))
                        .Append("\nItem ").Append(row.ItemID.ToString(CultureInfo.InvariantCulture))
                        .Append("  |  Quantity ").Append(row.Quantity.ToString(CultureInfo.InvariantCulture))
                        .Append("  |  Price ").Append(row.Price.ToString(CultureInfo.InvariantCulture))
                        .Append("\nRegistered ").Append(row.RegisteredAt.ToString(CultureInfo.InvariantCulture));
                else
                    text.Append("LISTING ").Append(row.ListingID.ToString(CultureInfo.InvariantCulture))
                        .Append("   ITEM ").Append(row.ItemID.ToString(CultureInfo.InvariantCulture))
                        .Append("   QTY ").Append(row.Quantity.ToString(CultureInfo.InvariantCulture))
                        .Append("   PRICE ").Append(row.Price.ToString(CultureInfo.InvariantCulture))
                        .Append("   SELLER ").Append(row.SellerCharacterID.ToString(CultureInfo.InvariantCulture))
                        .Append("   REGISTERED ").Append(row.RegisteredAt.ToString(CultureInfo.InvariantCulture))
                        .Append("   STATUS ").Append(row.Status.ToString(CultureInfo.InvariantCulture));
            }
            return text.ToString();
        }

        private static string BuildStatus(int count, bool myList, ushort itemType) => myList
            ? count == 0 ? "You have no listings." : "My listings: " + count.ToString(CultureInfo.InvariantCulture) + " listings · up to 30 shown"
            : count == 0 ? "No listings in " + ItemTypeName(itemType) + " on this page."
                : count.ToString(CultureInfo.InvariantCulture) + " listings · " + ItemTypeName(itemType);

        private static string ItemTypeName(ushort value) => value == 1 ? "potions" : value == 2 ? "weapons / shields" : "armor";
        private static string ListingStatusName(byte status) => status == 0 ? "Trading" : status == 1 ? "Sold" : "Claimed";

        private static string Describe(List<BazaarListingView> rows)
        {
            var text = new StringBuilder();
            foreach (BazaarListingView row in rows)
            {
                if (text.Length != 0) text.Append('|');
                text.Append(row.ListingID.ToString(CultureInfo.InvariantCulture)).Append(',')
                    .Append(row.SellerCharacterID.ToString(CultureInfo.InvariantCulture)).Append(',')
                    .Append(row.ItemID.ToString(CultureInfo.InvariantCulture)).Append(',')
                    .Append(row.Quantity.ToString(CultureInfo.InvariantCulture)).Append(',')
                    .Append(row.Price.ToString(CultureInfo.InvariantCulture)).Append(',')
                    .Append(row.RegisteredAt.ToString(CultureInfo.InvariantCulture)).Append(',')
                    .Append(row.Status.ToString(CultureInfo.InvariantCulture));
            }
            return text.ToString();
        }

        private static string State() => string.Format(CultureInfo.InvariantCulture,
            "stage={0} zone={1} internal={2} x={3:R} y={4:R} HP={5} MP={6}", world.Stage, world.ZoneID,
            world.InternalID, world.ServerPosition.x, world.ServerPosition.y, world.OwnFull.hp, world.OwnFull.mp);

        private static string SafeText(string value)
        {
            if (string.IsNullOrEmpty(value)) return "<empty>";
            value = Regex.Replace(value, "[\\r\\n\\t]+", " ");
            return value.Length <= 240 ? value : value.Substring(0, 240);
        }

        private static T Read<T>(object target, string field) => (T)target.GetType()
            .GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);

        private static void ValidateRunId(string value)
        {
            if (!UnityEditor.EditorApplication.isPlaying || !Regex.IsMatch(value ?? string.Empty, "^[A-Za-z0-9-]{1,40}$"))
                throw new InvalidOperationException("Play mode and a valid run ID are required.");
        }

        private static void ValidateLabel(string value)
        {
            if (!Regex.IsMatch(value ?? string.Empty, "^[A-Za-z0-9-]{1,40}$"))
                throw new ArgumentException("Capture labels may contain only letters, digits, and hyphens.", nameof(value));
        }

        private static string PathFor(string name) => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", name));
        private static void Append(string line) => File.AppendAllText(output, line + Environment.NewLine);
    }
}
#endif
