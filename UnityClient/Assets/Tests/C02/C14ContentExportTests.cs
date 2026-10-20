#if UNITY_EDITOR
using System;
using System.IO;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace C02.Tests
{
    public sealed class C14ContentExportTests
    {
        [Test]
        [Category("CatalogExport")]
        public void ExistingSkillAndMonsterCatalogExportsInIdOrderAndPreservesEdits()
        {
            var content = ScriptableObject.CreateInstance<C14ServerContent>();
            string directory = Path.Combine(Path.GetTempPath(), "C14Catalog-" + Guid.NewGuid().ToString("N"));
            try
            {
                content.ring.mana = 27;
                content.monster0.maxHp = 1700;
                Export(content, directory);
                string skills = File.ReadAllText(Path.Combine(directory, "Skills.inc"));
                string monsters = File.ReadAllText(Path.Combine(directory, "Monsters.inc"));
                int skill0 = FirstSkillId(skills, 0), skill1 = FirstSkillId(skills, 1), skill2 = FirstSkillId(skills, 2);
                int skill3 = FirstSkillId(skills, 3), skill4 = FirstSkillId(skills, 4);
                Assert.That(new[] { skill0, skill1, skill2, skill3, skill4 }, Is.All.GreaterThanOrEqualTo(0));
                Assert.That(skill0, Is.LessThan(skill1));
                Assert.That(skill1, Is.LessThan(skill2));
                Assert.That(skill2, Is.LessThan(skill3));
                Assert.That(skill3, Is.LessThan(skill4));
                Assert.That(skills, Does.Contain("    1, 2,\n    27, 120,"));
                int monster0 = FirstMonsterId(monsters, 0), monster1 = FirstMonsterId(monsters, 1), monster2 = FirstMonsterId(monsters, 2);
                Assert.That(new[] { monster0, monster1, monster2 }, Is.All.GreaterThanOrEqualTo(0));
                Assert.That(monster0, Is.LessThan(monster1));
                Assert.That(monster1, Is.LessThan(monster2));
                Assert.That(monsters, Does.Contain("    1700,"));
                string firstSkills = File.ReadAllText(Path.Combine(directory, "Skills.inc"));
                string firstMonsters = File.ReadAllText(Path.Combine(directory, "Monsters.inc"));
                Export(content, directory);
                Assert.That(File.ReadAllText(Path.Combine(directory, "Skills.inc")), Is.EqualTo(firstSkills));
                Assert.That(File.ReadAllText(Path.Combine(directory, "Monsters.inc")), Is.EqualTo(firstMonsters));
                Assert.That(Directory.GetFiles(directory).Length, Is.EqualTo(2));
            }
            finally
            {
                UnityEngine.Object.DestroyImmediate(content);
                if (Directory.Exists(directory)) Directory.Delete(directory, true);
            }
        }

        [Test]
        [Category("CatalogExport")]
        public void InvalidCatalogIdsPhasesAndHpLeaveBothOutputsUntouched()
        {
            var content = ScriptableObject.CreateInstance<C14ServerContent>();
            string directory = Path.Combine(Path.GetTempPath(), "C14Catalog-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            var skillsBefore = new byte[] { 10, 20, 30 };
            var monstersBefore = new byte[] { 40, 50, 60 };
            File.WriteAllBytes(Path.Combine(directory, "Skills.inc"), skillsBefore);
            File.WriteAllBytes(Path.Combine(directory, "Monsters.inc"), monstersBefore);
            try
            {
                Action assertPreserved = () =>
                {
                    var failure = Assert.Throws<TargetInvocationException>(() => Export(content, directory));
                    Assert.That(failure.InnerException, Is.TypeOf<InvalidOperationException>());
                    Assert.That(File.ReadAllBytes(Path.Combine(directory, "Skills.inc")), Is.EqualTo(skillsBefore));
                    Assert.That(File.ReadAllBytes(Path.Combine(directory, "Monsters.inc")), Is.EqualTo(monstersBefore));
                    Assert.That(Directory.GetFiles(directory).Length, Is.EqualTo(2));
                };

                content.otherSkills[0].skillID = 1;
                assertPreserved();
                content.otherSkills[0].skillID = 0;
                content.otherSkills[2].phases[1].action = C14SkillAction.Charging;
                assertPreserved();
                content.otherSkills[2].phases[1].action = C14SkillAction.Hit;
                content.monster0.maxHp = 2147483648u;
                assertPreserved();
                content.monster0.maxHp = 1500;
                content.monster0.respawnTicks = 0;
                assertPreserved();
                content.monster0.respawnTicks = 300;
                content.monster0.phases = new[]
                {
                    new C14ServerContent.MonsterPhase { hpThreshold = 0.0001f },
                    new C14ServerContent.MonsterPhase { hpThreshold = 0.00001f }
                };
                assertPreserved();
                content.monster0.phases[0].hpThreshold = 0.001f;
                assertPreserved();
                content.monster0.phases[0].hpThreshold = 1;
                Assert.DoesNotThrow(() => Export(content, directory));
            }
            finally
            {
                UnityEngine.Object.DestroyImmediate(content);
                Directory.Delete(directory, true);
            }
        }

        private static int FirstSkillId(string text, int id)
        {
            return text.IndexOf("SkillData{\n    " + id + ",", StringComparison.Ordinal);
        }

        private static int FirstMonsterId(string text, int id)
        {
            return text.IndexOf("{\n    " + id + ",", StringComparison.Ordinal);
        }

        private static void Export(C14ServerContent content, string directory)
        {
            var exporter = Type.GetType("C02.Editor.C14ServerContentExport, C02.Editor", true);
            exporter.GetMethod("ExportValidated", new[] { typeof(C14ServerContent), typeof(string) })
                .Invoke(null, new object[] { content, directory });
        }
    }
}
#endif
