#if UNITY_EDITOR
using System;
using System.Globalization;
using System.IO;
using System.Text;
using NUnit.Framework;
using UnityEditor;
using UnityEngine;

namespace Mmorpg.Client.Tests
{
    public sealed class ContentImportTests
    {
        [Test]
        public void SharedCsvLoadsCatalogsAndHuntingOverridesWithoutChangingAssetIdentity()
        {
            string assetPath = "Assets/Client/Content/CsvContentTest-" + Guid.NewGuid().ToString("N") + ".asset";
            var content = ScriptableObject.CreateInstance<ServerContent>();
            try
            {
                AssetDatabase.CreateAsset(content, assetPath);
                content.name = "Existing content reference";
                string guid = AssetDatabase.AssetPathToGUID(assetPath);
                byte[] metadata = File.ReadAllBytes(assetPath + ".meta");
                content.LoadCsv(SharedGameData.DirectoryPath);

                Assert.That(content.otherSkills.Length + 1, Is.EqualTo(RecordCount("Skills/PlayerSkills.csv")));
                Assert.That(content.monsterSkills.Length, Is.EqualTo(RecordCount("Skills/MonsterSkills.csv")));
                Assert.That(content.otherMonsters.Length + 1, Is.EqualTo(RecordCount("Monsters/Monsters.csv")));
                Assert.That(content.monsterSpecies.Length, Is.EqualTo(RecordCount("Monsters/HuntingSpecies.csv")));
                Assert.That(content.huntingZones.Length, Is.EqualTo(RecordCount("Monsters/HuntingZones.csv")));
                Assert.That(content.growth.levels.Length, Is.EqualTo(RecordCount("Growth/Growth.csv")));
                Assert.That(content.growth.maxLevel, Is.EqualTo(100));
                int itemCount = RecordCount("Items/Potions.csv") + RecordCount("Items/Weapons.csv") +
                    RecordCount("Items/Shields.csv") + RecordCount("Items/Armors.csv");
                Assert.That(content.items.Length, Is.EqualTo(itemCount));

                Assert.That(content.ring.cooldownTicks, Is.EqualTo(Number("Skills/PlayerSkills.csv", "skillID", "1", "coolDown")));
                Assert.That(content.ring.phases[1].waitTicks, Is.EqualTo(16));
                Assert.That(content.ring.phases[1].range, Is.EqualTo(SkillRange.Circle));
                Assert.That(content.GetItem(1).hpRecovery, Is.EqualTo(Number("Items/Potions.csv", "itemID", "1", "recovery")));
                Assert.That(content.GetItem(1).mpRecovery, Is.Zero);
                Assert.That(content.GetItem(2).mpRecovery, Is.EqualTo(Number("Items/Potions.csv", "itemID", "2", "recovery")));
                Assert.That(content.GetItem(3).weaponAttackBonus, Is.EqualTo(Number("Items/Weapons.csv", "itemID", "3", "attackBonus")));
                Assert.That(content.GetItem(4).defensePercent, Is.EqualTo(Number("Items/Shields.csv", "itemID", "4", "defensePercent")));
                Assert.That(content.GetItem(5).defensePercent, Is.EqualTo(Number("Items/Armors.csv", "itemID", "5", "defensePercent")));
                Assert.That(content.GetItem(33), Is.Null);
                Assert.That(ItemPresentation.Name(3, content), Is.EqualTo(content.GetItem(3).name));
                Assert.That(content.GetHuntingZone(1).minLevel, Is.Zero);

                var altarElite = content.GetMonsterSpecies(12, 6);
                Assert.That(altarElite, Is.Not.Null);
                Assert.That(altarElite.monsterType, Is.EqualTo(1));
                Assert.That(altarElite.attack, Is.EqualTo(Number("Monsters/HuntingZones.csv", "zoneID", "6", "strongAttack")));
                Assert.That(altarElite.phases[0].steps[1].skillID, Is.EqualTo(2));
                Assert.That(altarElite.phases[0].steps[2].skillID, Is.EqualTo(7));
                Assert.That(altarElite.phases[0].steps[2].delayTicks, Is.EqualTo(26));
                Assert.That(content.otherMonsters[0].phases[0].steps[2].skillID, Is.EqualTo(1),
                    "A hunting override must not overwrite the shared base archetype.");
                Assert.That(content.GetMonsterSpecies(2, 4).attack, Is.Not.EqualTo(content.GetMonsterSpecies(2, 6).attack),
                    "A boss species ID is resolved together with its zone.");

                string firstLoad = EditorJsonUtility.ToJson(content);
                content.LoadCsv(SharedGameData.DirectoryPath);
                Assert.That(EditorJsonUtility.ToJson(content), Is.EqualTo(firstLoad));
                Assert.That(content.name, Is.EqualTo("Existing content reference"));
                Assert.That(AssetDatabase.AssetPathToGUID(assetPath), Is.EqualTo(guid));
                Assert.That(File.ReadAllBytes(assetPath + ".meta"), Is.EqualTo(metadata));
            }
            finally
            {
                AssetDatabase.DeleteAsset(assetPath);
                if (content != null) UnityEngine.Object.DestroyImmediate(content);
            }
        }

        [Test]
        public void CsvAcceptsBomQuotedMultilineNamesAndExplicitSpeciesTypes()
        {
            string directory = NewFixtureDirectory();
            var content = ScriptableObject.CreateInstance<ServerContent>();
            try
            {
                CopySharedData(directory);
                string path = Path.Combine(directory, "Monsters/HuntingSpecies.csv");
                string original = File.ReadAllText(path);
                string name = "성문, 순찰 \"대장\"\n둘째 줄";
                string replacement = "501,2,0,\"성문, 순찰 \"\"대장\"\"\n둘째 줄\"";
                Assert.That(original, Does.Contain("3,2,0,성문 순찰병"));
                File.WriteAllText(path, original.Replace("3,2,0,성문 순찰병", replacement), new UTF8Encoding(true));
                ReplaceCell(directory, "Monsters/HuntingZones.csv", "zoneID", "2", "regularGold", "4294967418");

                content.LoadCsv(directory);

                var species = content.GetMonsterSpecies(501, 2);
                Assert.That(species, Is.Not.Null);
                Assert.That(species.name, Is.EqualTo(name));
                Assert.That(species.monsterType, Is.Zero, "Visual archetypes come from monsterType, not species ID parity.");
                Assert.That(content.GetMonsterSpecies(3, 2), Is.Null);
                Assert.That(content.GetHuntingZone(2).regularGold, Is.EqualTo(4294967418UL));
                Assert.That(content.GetMonsterSpecies(2, 4), Is.Not.Null);
                Assert.That(content.GetMonsterSpecies(2, 6), Is.Not.Null);
            }
            finally
            {
                Directory.Delete(directory, true);
                UnityEngine.Object.DestroyImmediate(content);
            }
        }

        [Test]
        public void InvalidCsvKeepsAllPriorCatalogReferencesAndAssetMetadata()
        {
            string directory = NewFixtureDirectory();
            string assetPath = "Assets/Client/Content/CsvContentTest-" + Guid.NewGuid().ToString("N") + ".asset";
            var content = ScriptableObject.CreateInstance<ServerContent>();
            try
            {
                content.LoadCsv(SharedGameData.DirectoryPath);
                AssetDatabase.CreateAsset(content, assetPath);
                AssetDatabase.SaveAssetIfDirty(content);
                content.ring.cooldownTicks = 543;
                EditorUtility.SetDirty(content);
                string before = EditorJsonUtility.ToJson(content);
                byte[] persisted = File.ReadAllBytes(assetPath);
                byte[] metadata = File.ReadAllBytes(assetPath + ".meta");
                string guid = AssetDatabase.AssetPathToGUID(assetPath);
                var ring = content.ring;
                var monster = content.monster0;
                var skills = content.otherSkills;
                var monsters = content.otherMonsters;
                var growth = content.growth;
                var monsterSkills = content.monsterSkills;
                var species = content.monsterSpecies;
                var huntingZones = content.huntingZones;
                var items = content.items;
                var parry = content.parry;
                Action<string>[] invalidate =
                {
                    dir => File.AppendAllText(Path.Combine(dir, "Items/Weapons.csv"), "\n1,Duplicate item,1,2\n"),
                    dir => ReplaceCell(dir, "Items/Potions.csv", "itemID", "1", "recovery", "1.5"),
                    dir => File.WriteAllText(Path.Combine(dir, "Growth/Growth.csv"), "level,requiredTotalExp,maxHpIncrease\n1,0,0\n"),
                    dir => ReplaceCell(dir, "Skills/MonsterSkillPhases.csv", "skillID", "0", "range1", "NaN"),
                    dir => File.AppendAllText(Path.Combine(dir, "Monsters/HuntingSkillOverrides.csv"), "\n6,1,0,0,65535,20\n"),
                    dir => File.AppendAllText(Path.Combine(dir, "Monsters/HuntingSpecies.csv"), "\n501,2,0,Duplicate type\n"),
                    dir => File.WriteAllText(Path.Combine(dir, "Monsters/HuntingSpecies.csv"), "speciesID,zoneID,monsterType,name\n501,2,0,\"unterminated\n")
                };
                foreach (var mutation in invalidate)
                {
                    CopySharedData(directory);
                    mutation(directory);
                    Assert.Throws<InvalidDataException>(() => content.LoadCsv(directory));
                    Assert.That(EditorJsonUtility.ToJson(content), Is.EqualTo(before));
                    Assert.That(content.ring, Is.SameAs(ring));
                    Assert.That(content.monster0, Is.SameAs(monster));
                    Assert.That(content.otherSkills, Is.SameAs(skills));
                    Assert.That(content.otherMonsters, Is.SameAs(monsters));
                    Assert.That(content.growth, Is.SameAs(growth));
                    Assert.That(content.monsterSkills, Is.SameAs(monsterSkills));
                    Assert.That(content.monsterSpecies, Is.SameAs(species));
                    Assert.That(content.huntingZones, Is.SameAs(huntingZones));
                    Assert.That(content.items, Is.SameAs(items));
                    Assert.That(content.parry, Is.SameAs(parry));
                    Assert.That(EditorUtility.IsDirty(content), Is.True);
                    Assert.That(File.ReadAllBytes(assetPath), Is.EqualTo(persisted));
                    Assert.That(File.ReadAllBytes(assetPath + ".meta"), Is.EqualTo(metadata));
                    Assert.That(AssetDatabase.AssetPathToGUID(assetPath), Is.EqualTo(guid));
                }
            }
            finally
            {
                Directory.Delete(directory, true);
                AssetDatabase.DeleteAsset(assetPath);
                if (content != null) UnityEngine.Object.DestroyImmediate(content);
            }
        }

        // These helpers inspect the repository's plain numeric rows; the runtime loader itself
        // is exercised above with quoted multiline records and a BOM.
        private static int RecordCount(string relativePath) => File.ReadAllLines(Path.Combine(SharedGameData.DirectoryPath, relativePath)).Length - 1;

        private static ulong Number(string relativePath, string key, string value, string column)
        {
            string[] lines = File.ReadAllLines(Path.Combine(SharedGameData.DirectoryPath, relativePath));
            string[] columns = lines[0].Split(',');
            int keyIndex = Array.IndexOf(columns, key);
            int columnIndex = Array.IndexOf(columns, column);
            foreach (string line in lines)
            {
                string[] fields = line.Split(',');
                if (fields[keyIndex] == value) return ulong.Parse(fields[columnIndex], CultureInfo.InvariantCulture);
            }
            throw new InvalidOperationException("Expected shared CSV fixture row was not found.");
        }

        private static void ReplaceCell(string directory, string relativePath, string key, string value, string column, string replacement)
        {
            string path = Path.Combine(directory, relativePath);
            string[] lines = File.ReadAllLines(path);
            string[] columns = lines[0].Split(',');
            int keyIndex = Array.IndexOf(columns, key);
            int columnIndex = Array.IndexOf(columns, column);
            for (int i = 1; i < lines.Length; i++)
            {
                string[] fields = lines[i].Split(',');
                if (fields[keyIndex] != value) continue;
                fields[columnIndex] = replacement;
                lines[i] = string.Join(",", fields);
                File.WriteAllLines(path, lines, new UTF8Encoding(false));
                return;
            }
            throw new InvalidOperationException("Expected CSV mutation row was not found.");
        }

        private static string NewFixtureDirectory()
        {
            string directory = Path.Combine(Application.temporaryCachePath, "SharedCsvTest-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            return directory;
        }

        private static void CopySharedData(string destination)
        {
            string source = SharedGameData.DirectoryPath;
            foreach (string path in Directory.GetFiles(source, "*.csv", SearchOption.AllDirectories))
            {
                string copy = Path.Combine(destination, path.Substring(source.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar).Length + 1));
                Directory.CreateDirectory(Path.GetDirectoryName(copy));
                File.Copy(path, copy, true);
            }
        }
    }
}
#endif
