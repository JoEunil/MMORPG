#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using C01;
using ClientCore;
using NUnit.Framework;
using TMPro;
using UnityEngine;

namespace C02.Tests
{
    // Physical Windows input drives the UI. This helper observes only actual ViewModel responses.
    public static class C09LiveBazaarUiTests
    {
        private static C01LoginScreen login;
        private static C02WorldScreen world;
        private static C09BazaarScreen screen;
        private static IViewModelUI model;
        private static string output, id, observationFailure;
        private static Action<bool> observer;
        private static Vector2 position;
        private static int hp, mp, responses, lastRows;
        private static bool lastSuccess;
        private static readonly ushort[] expectedTypes = { 1, 2, 3, 3, 3, 3, 3 };
        private static readonly ushort[] expectedPages = { 0, 0, 0, 1, 1, 0, 0 };

        public static void Begin(string runId)
        {
            ValidateRunId(runId);
            if (model != null) throw new InvalidOperationException("A C09 observation is already active.");
            login = GameObject.Find("C01Client").GetComponent<C01LoginScreen>();
            world = GameObject.Find("C02Client").GetComponent<C02WorldScreen>();
            screen = UnityEngine.Object.FindFirstObjectByType<C09BazaarScreen>();
            Assert.That(login.Model != null && login.Data != null && world.Stage == C02Stage.InWorld &&
                world.SelectedCharacterID == 5011 && world.HasOwnFull && world.IsWorldVisible && screen != null && !screen.IsOpen, Is.True,
                "Begin requires the logged-in dedicated character and confirmed visible field.");

            id = runId;
            output = PathFor("c09-physical-" + id + ".txt");
            if (File.Exists(output)) throw new IOException("Earlier observation evidence exists.");
            position = world.ServerPosition;
            hp = world.OwnFull.hp;
            mp = world.OwnFull.mp;
            responses = 0;
            lastRows = 0;
            lastSuccess = false;
            observationFailure = null;
            model = login.Model;
            File.WriteAllText(output, "Physical Windows mouse input; passive observation of actual bazaar search responses.\n" +
                "Successful empty SELECT results are valid. UI rows are compared with the received ViewData snapshot.\n");
            observer = Observe;
            model.OnBazaarReceived += observer;
            Append("BEGIN " + State());
        }

        private static void Observe(bool success)
        {
            try
            {
                if (login.Model != model) throw new InvalidOperationException("Client changed during the observation.");
                responses++;
                lastSuccess = success;
                List<BazaarListingView> data = login.Data.GetBazaar();
                lastRows = data.Count;
                ushort type = Read<ushort>(screen, "itemType");
                ushort page = Read<ushort>(screen, "page");
                string rows = Read<TMP_Text>(screen, "rowsText").text;
                string status = Read<TMP_Text>(screen, "statusText").text;
                string pageLabel = Read<TMP_Text>(screen, "pageText").text;
                string expectedRows = success ? BuildRows(data) : string.Empty;
                var record = new StringBuilder()
                    .Append("RESPONSE ").Append(responses)
                    .Append(" success=").Append(success)
                    .Append(" rows=").Append(data.Count)
                    .Append(" type=").Append(type)
                    .Append(" page=").Append(page)
                    .Append(" pageLabel=").Append(pageLabel)
                    .Append(" open=").Append(screen.IsOpen)
                    .Append(" status=").Append(status)
                    .Append(" data=").Append(expectedRows.Replace('\n', '|'))
                    .Append(" uiMatch=").Append(rows == expectedRows)
                    .Append(" ").Append(State()).ToString();
                Append(record);
                if (!success) throw new InvalidOperationException("The real search response reported failure.");
                if (data.Count > 30 || rows != expectedRows) throw new InvalidOperationException("The displayed rows differ from the actual response data.");
                if (screen.IsOpen == false || type < 1 || type > 3) throw new InvalidOperationException("Response arrived outside the open market screen.");
                if (responses > expectedTypes.Length || type != expectedTypes[responses - 1] || page != expectedPages[responses - 1])
                    throw new InvalidOperationException("The response did not match the expected type/page sequence.");
                if (pageLabel != "Page " + ((uint)page + 1).ToString(CultureInfo.InvariantCulture))
                    throw new InvalidOperationException("The page label does not match the requested page.");
                if (data.Count == 0 && !status.Contains("No listings")) throw new InvalidOperationException("Successful empty response was not shown as an empty page.");
                if (data.Count > 0 && !status.Contains(data.Count.ToString(CultureInfo.InvariantCulture))) throw new InvalidOperationException("Displayed row count does not match the server data.");
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
            Assert.That(expectedResponses >= 1 && expectedResponses <= expectedTypes.Length && responses == expectedResponses, Is.True,
                "Unexpected or missing server response count at " + label + ".");
            Assert.That(lastSuccess, Is.True, "Each real search, including an empty result, must be successful.");
            Assert.That(Read<ushort>(screen, "itemType"), Is.EqualTo(expectedTypes[expectedResponses - 1]));
            Assert.That(Read<ushort>(screen, "page"), Is.EqualTo(expectedPages[expectedResponses - 1]));
            Assert.That(screen.IsOpen && world.Stage == C02Stage.InWorld && world.HasOwnFull, Is.True);
            AssertWorldUnchanged();
            Append("CHECKPOINT " + label + " responses=" + responses + " success=" + lastSuccess + " rows=" + lastRows +
                " type=" + Read<ushort>(screen, "itemType") + " page=" + Read<ushort>(screen, "page") + " status=" + Read<TMP_Text>(screen, "statusText").text + " " + State());
            string path = PathFor("c09-physical-" + id + "-" + label + ".png");
            if (File.Exists(path)) throw new IOException("Earlier capture exists.");
            ScreenCapture.CaptureScreenshot(path);
            return path;
        }

        public static void CloseCheckpoint()
        {
            EnsureActive();
            Assert.That(observationFailure, Is.Null, observationFailure);
            Assert.That(screen.IsOpen, Is.False, "The physical Close button should hide the bazaar.");
            Assert.That(responses, Is.EqualTo(6), "Close follows the six type/page/refresh searches.");
            Assert.That(Read<TMP_Text>(screen, "rowsText").text, Is.Empty, "Close clears the visible listing rows.");
            AssertWorldUnchanged();
            Append("PASS physical close: panel closed, rows cleared, position/HP/MP unchanged. " + State());
        }

        public static void Finish()
        {
            try
            {
                Assert.That(observationFailure, Is.Null, observationFailure);
                Assert.That(responses >= 7, Is.True, "The complete type/page/refresh/reopen path must receive at least seven server responses.");
                Assert.That(login.Model == null && !world.IsWorldVisible, Is.True, "Finish requires normal UI logout and hidden world.");
                Append("PASS seven or more actual successful searches; successful empty results accepted; normal UI logout. " + State());
            }
            finally
            {
                if (model != null && observer != null) model.OnBazaarReceived -= observer;
                model = null;
                observer = null;
            }
        }

        private static void EnsureActive()
        {
            if (model == null || login == null || world == null || screen == null || login.Model != model)
                throw new InvalidOperationException("Start a fresh C09 observation before using checkpoints.");
        }

        private static void AssertWorldUnchanged()
        {
            Assert.That(world.ServerPosition, Is.EqualTo(position), "The market UI must not move the character.");
            Assert.That(world.OwnFull.hp, Is.EqualTo(hp), "HP changed during the read-only market flow.");
            Assert.That(world.OwnFull.mp, Is.EqualTo(mp), "MP changed during the read-only market flow.");
        }

        private static string BuildRows(List<BazaarListingView> rows)
        {
            var text = new StringBuilder(rows.Count * 100);
            foreach (BazaarListingView row in rows)
            {
                if (text.Length != 0) text.Append('\n');
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

        private static string State() => string.Format(CultureInfo.InvariantCulture,
            "stage={0} zone={1} internal={2} x={3:R} y={4:R} HP={5} MP={6}", world.Stage, world.ZoneID,
            world.InternalID, world.ServerPosition.x, world.ServerPosition.y, world.OwnFull.hp, world.OwnFull.mp);

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
