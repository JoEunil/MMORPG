#if UNITY_EDITOR
using System;
using System.IO;
using System.Reflection;
using C02;
using NUnit.Framework;
using UnityEditor;
using UnityEngine;
using Object = UnityEngine.Object;

namespace C02.Tests
{
    public sealed class C20MapExportTests
    {
        [Test]
        public void BakeAndExportUsesTheSameObstacleSnapshot()
        {
            var author = CreateAuthor();
            string directory = Path.Combine(Path.GetTempPath(), "C20Export-" + Guid.NewGuid().ToString("N"));
            try
            {
                BakeAndExport(author, directory);
                Assert.That(author.map.walkable.Length, Is.EqualTo(196));
                for (int i = 0; i < 196; i++)
                    Assert.That(author.map.walkable[i], Is.EqualTo(i == 15 ? 0 : 1), "tile " + i);
                Assert.That(File.ReadAllText(Path.Combine(directory, "WorldMap.inc")),
                    Does.Contain("0xFFFFFFFFFFFF7FFFULL,"));
                var layout = C24WorldLayout.Build(3, 3, 4, 1);
                Assert.That(layout.Zones.Length, Is.EqualTo(9));
                Assert.That(layout.Zones[4].AoiCells.Length, Is.EqualTo(25));
                Assert.That(layout.LocalToWorld(9, Vector2.zero, out var origin), Is.True);
                Assert.That(origin, Is.EqualTo(new Vector2(7, 7)));
                Assert.That(layout.WorldToLocal(9, origin, out var local), Is.True);
                Assert.That(local, Is.EqualTo(Vector2.zero));
                Assert.That(layout.LocalToWorld(9, new Vector2(6, 0), out _), Is.False);
                Assert.That(EditorUtility.IsDirty(author.map), Is.True);
                Assert.That(Directory.GetFiles(directory).Length, Is.EqualTo(1));
            }
            finally
            {
                Object.DestroyImmediate(author.map);
                Object.DestroyImmediate(author.gameObject);
                if (Directory.Exists(directory)) Directory.Delete(directory, true);
            }
        }

        [Test]
        public void FailedExportPreservesPreviousMapAndDirtyState()
        {
            var author = CreateAuthor();
            string directory = Path.Combine(Path.GetTempPath(), "C20Export-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            string output = Path.Combine(directory, "WorldMap.inc");
            byte[] originalFile = { 1, 3, 5, 7 };
            File.WriteAllBytes(output, originalFile);
            try
            {
                File.SetAttributes(output, FileAttributes.ReadOnly);
                foreach (bool dirty in new[] { false, true })
                {
                    byte[] previous = author.map.walkable;
                    byte[] previousContents = (byte[])previous.Clone();
                    if (dirty) EditorUtility.SetDirty(author.map);
                    else EditorUtility.ClearDirty(author.map);
                    var failure = Assert.Throws<TargetInvocationException>(() => BakeAndExport(author, directory));
                    Assert.That(failure.InnerException, Is.TypeOf<UnauthorizedAccessException>());
                    Assert.That(author.map.walkable, Is.SameAs(previous));
                    Assert.That(author.map.walkable, Is.EqualTo(previousContents));
                    Assert.That(EditorUtility.IsDirty(author.map), Is.EqualTo(dirty));
                    Assert.That(author.map.origin, Is.EqualTo(new Vector2(-1, -1)));
                    Assert.That(File.ReadAllBytes(output), Is.EqualTo(originalFile));
                    Assert.That(Directory.GetFiles(directory).Length, Is.EqualTo(1));
                }
            }
            finally
            {
                File.SetAttributes(output, FileAttributes.Normal);
                Directory.Delete(directory, true);
                Object.DestroyImmediate(author.map);
                Object.DestroyImmediate(author.gameObject);
            }
        }

        [Test]
        public void RejectsBlockedOverlapAndReportsBlockedDiagonal()
        {
            var author = CreateAuthor();
            string directory = Path.Combine(Path.GetTempPath(), "C24Blocked-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            string output = Path.Combine(directory, "WorldMap.inc");
            File.WriteAllText(output, "previous complete snapshot");
            try
            {
                author.zoneColumns = 2; author.zoneRows = 1;
                author.zoneCoreSize = 8; author.transitionMargin = 2;
                author.map.origin = new Vector2(-2, -2);
                author.map.width = 20; author.map.height = 12;
                author.ground.center = new Vector3(8, -.1f, 4);
                author.ground.size = new Vector3(20, .1f, 12);
                author.obstacles[0].center = new Vector3(6.1f, 0, 4);
                author.obstacles[0].size = new Vector3(.1f, 1, 12);
                var previous = author.map.walkable;
                var failure = Assert.Throws<TargetInvocationException>(() => BakeAndExport(author, directory));
                Assert.That(failure.InnerException, Is.TypeOf<InvalidOperationException>());
                Assert.That(failure.InnerException.Message, Does.Contain("Zones 1 and 2"));
                Assert.That(author.map.walkable, Is.SameAs(previous));
                Assert.That(File.ReadAllText(output), Is.EqualTo("previous complete snapshot"));

                author.zoneColumns = author.zoneRows = 3;
                author.map.width = author.map.height = 28;
                author.ground.center = new Vector3(12, -.1f, 12);
                author.ground.size = new Vector3(28, .1f, 28);
                author.obstacles[0].center = new Vector3(8, 0, 8);
                author.obstacles[0].size = new Vector3(4, 1, 4);
                BakeAndExport(author, directory);
                string exported = File.ReadAllText(output);
                Assert.That(exported, Does.Contain("directCrossings"));
                // Zone 1 retains geometric neighbor 5, but its NE ownership corner is blocked.
                Assert.That(exported, Does.Contain("{{false, false, false, false, true, false, true, false}}}"));
                Assert.That(exported, Does.Contain("{{0, 0, 0, 0, 2, 0, 4, 5}}"));
            }
            finally
            {
                Object.DestroyImmediate(author.map);
                Object.DestroyImmediate(author.gameObject);
                Directory.Delete(directory, true);
            }
        }

        private static C20MapAuthoring CreateAuthor()
        {
            var author = new GameObject("Detached map export fixture").AddComponent<C20MapAuthoring>();
            author.map = ScriptableObject.CreateInstance<C20WorldMap>();
            author.zoneColumns = author.zoneRows = 3;
            author.zoneCoreSize = 4;
            author.transitionMargin = 1;
            author.map.origin = new Vector2(-1, -1);
            author.map.width = author.map.height = 14;
            author.map.walkable = new byte[196];
            author.actorClearance = 0;
            author.ground = new GameObject("Ground").AddComponent<BoxCollider>();
            author.ground.transform.SetParent(author.transform, false);
            author.ground.center = new Vector3(6, -.1f, 6);
            author.ground.size = new Vector3(14, .1f, 14);
            var obstacle = new GameObject("Wall").AddComponent<BoxCollider>();
            obstacle.transform.SetParent(author.transform, false);
            obstacle.center = new Vector3(.5f, 0, .5f);
            obstacle.size = new Vector3(.2f, 1, .2f);
            author.obstacles = new[] { obstacle };
            return author;
        }

        private static void BakeAndExport(C20MapAuthoring author, string directory)
        {
            var exporter = Type.GetType("C20WorldMapExport, C02.Editor", true);
            exporter.GetMethod("BakeAndExport", BindingFlags.Public | BindingFlags.Static,
                null, new[] { typeof(C20MapAuthoring), typeof(string) }, null)
                .Invoke(null, new object[] { author, directory });
        }
    }
}
#endif
