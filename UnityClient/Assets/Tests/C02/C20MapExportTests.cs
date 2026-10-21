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
                Assert.That(author.map.walkable.Length, Is.EqualTo(16));
                for (int i = 0; i < 16; i++)
                    Assert.That(author.map.walkable[i], Is.EqualTo(i == 5 ? 0 : 1), "tile " + i);
                Assert.That(File.ReadAllText(Path.Combine(directory, "WorldMap.inc")),
                    Does.Contain("0x000000000000FFDFULL,"));
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
                    Assert.That(author.map.origin, Is.EqualTo(Vector2.zero));
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

        private static C20MapAuthoring CreateAuthor()
        {
            var author = new GameObject("Detached map export fixture").AddComponent<C20MapAuthoring>();
            author.map = ScriptableObject.CreateInstance<C20WorldMap>();
            author.map.origin = Vector2.zero;
            author.map.width = author.map.height = 4;
            author.map.walkable = new byte[16];
            author.actorClearance = 0;
            author.ground = new GameObject("Ground").AddComponent<BoxCollider>();
            author.ground.transform.SetParent(author.transform, false);
            author.ground.center = new Vector3(2, -.1f, 2);
            author.ground.size = new Vector3(4, .1f, 4);
            var obstacle = new GameObject("Wall").AddComponent<BoxCollider>();
            obstacle.transform.SetParent(author.transform, false);
            obstacle.center = new Vector3(1.5f, 0, 1.5f);
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
