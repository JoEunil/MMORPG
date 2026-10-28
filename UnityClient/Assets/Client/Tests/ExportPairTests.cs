#if UNITY_EDITOR
using System;
using System.IO;
using System.Reflection;
using NUnit.Framework;

namespace Mmorpg.Client.Tests
{
    public sealed class ExportPairTests
    {
        [Test]
        [Category("ExportPairReview")]
        public void ExportPairWritesBothOrRestoresOriginalFiles()
        {
            string directory = Path.Combine(Path.GetTempPath(), "C14ExportPair-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            string first = Path.Combine(directory, "Skills.inc");
            string second = Path.Combine(directory, "Monsters.inc");
            var originalFirst = new byte[] { 1, 2, 3 };
            var originalSecond = new byte[] { 4, 5, 6 };
            try
            {
                File.WriteAllBytes(first, originalFirst);
                File.WriteAllBytes(second, originalSecond);
                Write(directory, "Skills.inc", "new skills\n", "Monsters.inc", "new monsters\n");
                Assert.That(File.ReadAllText(first), Is.EqualTo("new skills\n"));
                Assert.That(File.ReadAllText(second), Is.EqualTo("new monsters\n"));
                Assert.That(Directory.GetFiles(directory).Length, Is.EqualTo(2));

                File.WriteAllBytes(first, originalFirst);
                File.WriteAllBytes(second, originalSecond);
                File.SetAttributes(second, File.GetAttributes(second) | FileAttributes.ReadOnly);
                Assert.Throws<TargetInvocationException>(() => Write(directory, "Skills.inc", "blocked\n", "Monsters.inc", "blocked\n"));
                Assert.That(File.ReadAllBytes(first), Is.EqualTo(originalFirst));
                Assert.That(File.ReadAllBytes(second), Is.EqualTo(originalSecond));
                File.SetAttributes(second, FileAttributes.Normal);

                File.Delete(second);
                Directory.CreateDirectory(second);
                Assert.Throws<TargetInvocationException>(() => Write(directory, "Skills.inc", "must rollback\n", "Monsters.inc", "blocked by directory\n"));
                Assert.That(File.ReadAllBytes(first), Is.EqualTo(originalFirst));
                Assert.That(Directory.Exists(second), Is.True);
                Assert.That(Directory.GetFiles(directory).Length, Is.EqualTo(1));

                File.Delete(first);
                Assert.Throws<TargetInvocationException>(() => Write(directory, "Skills.inc", "must remain absent\n", "Monsters.inc", "blocked by directory\n"));
                Assert.That(File.Exists(first), Is.False);
                Assert.That(Directory.Exists(second), Is.True);
                Assert.That(Directory.GetFiles(directory).Length, Is.EqualTo(0));
            }
            finally
            {
                if (File.Exists(second)) File.SetAttributes(second, FileAttributes.Normal);
                Directory.Delete(directory, true);
            }
        }

        private static void Write(string directory, string firstName, string firstText, string secondName, string secondText)
        {
            var helper = Type.GetType("Mmorpg.Client.Editor.ExportPair, Mmorpg.Client.Editor", true);
            helper.GetMethod("Write", BindingFlags.Public | BindingFlags.Static)
                .Invoke(null, new object[] { directory, firstName, firstText, secondName, secondText });
        }
    }
}
#endif
