#if UNITY_EDITOR
using System;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using C01;
using ClientCore;
using NUnit.Framework;
using UnityEngine;

namespace C02.Tests
{
    // Physical Windows input drives the UI. This helper observes server-confirmed results only.
    public static class C08LiveZoneUiTests
    {
        private static C01LoginScreen login;
        private static C02WorldScreen world;
        private static IViewModelUI model;
        private static string output, id;
        private static ushort lastZone;
        private static int transitions;
        private static bool awaiting;
        private static Vector2 before;
        private static int hp, mp;
        private static string observationFailure;

        public static void Begin(string runId)
        {
            if (!UnityEditor.EditorApplication.isPlaying || model != null || !Regex.IsMatch(runId ?? "", "^[A-Za-z0-9-]{1,40}$"))
                throw new InvalidOperationException("Play, a fresh observer and a valid run ID are required.");
            login = GameObject.Find("C01Client").GetComponent<C01LoginScreen>();
            world = GameObject.Find("C02Client").GetComponent<C02WorldScreen>();
            Assert.That(login.Model != null && world.Stage == C02Stage.InWorld && world.SelectedCharacterID == 5011 && world.HasOwnFull, Is.True);
            id = runId; output = PathFor("c08-physical-" + id + ".txt");
            if (File.Exists(output)) throw new IOException("Earlier evidence exists.");
            File.WriteAllText(output, "Physical mouse input; passive server-confirmed Zone transition observation.\n");
            model = login.Model; lastZone = world.ZoneID; transitions = 0; awaiting = false; observationFailure = null;
            world.WorldStateChanged += Observe;
            Append("BEGIN " + State());
        }

        private static void Observe()
        {
            try { ObserveState(); }
            catch (Exception exception)
            {
                // A passive observer must never interrupt the client's own disconnect/reset callbacks.
                observationFailure = exception.Message;
                Append("FAIL OBSERVATION " + observationFailure);
            }
        }

        private static void ObserveState()
        {
            if (login.Model != model) return;
            if (world.Stage == C02Stage.TransitionPending && !awaiting)
            {
                awaiting = true; before = world.ServerPosition; hp = world.OwnFull.hp; mp = world.OwnFull.mp;
                Assert.That(world.CanAct, Is.False);
                Append("REQUEST " + State());
            }
            if (world.Stage != C02Stage.InWorld || !world.HasOwnFull || world.ZoneID == lastZone) return;
            Assert.That(awaiting && world.HasOwnFull, Is.True);
            Assert.That(world.ZoneStartPosition, Is.EqualTo(before), "Zone ACK must preserve global position.");
            Assert.That(world.OwnFull.hp, Is.EqualTo(hp), "HP changed across the observed transfer.");
            Assert.That(world.OwnFull.mp, Is.EqualTo(mp), "MP changed across the observed transfer.");
            transitions++; lastZone = world.ZoneID; awaiting = false;
            Append("PASS ARRIVAL " + transitions + " " + State());
        }

        public static string Checkpoint(ushort zone)
        {
            Assert.That(login.Model == model && world.Stage == C02Stage.InWorld && world.HasOwnFull && world.ZoneID == zone, Is.True);
            Append("CHECKPOINT " + State());
            var path = PathFor("c08-physical-" + id + "-zone" + zone + ".png");
            if (File.Exists(path)) throw new IOException("Earlier capture exists.");
            ScreenCapture.CaptureScreenshot(path);
            return path;
        }

        public static void Finish()
        {
            try
            {
                Assert.That(observationFailure, Is.Null, "All observation assertions must have passed.");
                Assert.That(login.Model == null && !world.IsWorldVisible && transitions == 2 && !awaiting, Is.True);
                Append("PASS two actual Zone transitions and normal UI disconnect.");
            }
            finally { if (world != null) world.WorldStateChanged -= Observe; model = null; }
        }
        private static string State() => string.Format(CultureInfo.InvariantCulture,
            "stage={0} zone={1} internal={2} x={3:R} y={4:R} HP={5} MP={6}", world.Stage, world.ZoneID, world.InternalID,
            world.ServerPosition.x, world.ServerPosition.y, world.OwnFull.hp, world.OwnFull.mp);
        private static string PathFor(string name) => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", name));
        private static void Append(string text) => File.AppendAllText(output, text + Environment.NewLine);
    }
}
#endif
