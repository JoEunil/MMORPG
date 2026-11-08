#if UNITY_EDITOR
using System;
using System.IO;
using System.Reflection;
using Newtonsoft.Json.Linq;
using NUnit.Framework;
using UnityEditor;
using UnityEngine;

namespace Mmorpg.Client.Tests
{
    public sealed class ContentImportTests
    {
        [Test]
        [Category("ContentImportDirection")]
        public void ServerSnapshotImportsEntireCatalogDeterministicallyWithoutChangingAssetGuid()
        {
            string assetPath = "Assets/Client/Content/ContentImportTest-" + Guid.NewGuid().ToString("N") + ".asset";
            var content = ScriptableObject.CreateInstance<ServerContent>();
            try
            {
                AssetDatabase.CreateAsset(content, assetPath);
                content.name = "Preserved metadata";
                string guid = AssetDatabase.AssetPathToGUID(assetPath);
                byte[] metadata = File.ReadAllBytes(assetPath + ".meta");
                var expected = JObject.Parse(File.ReadAllText(SnapshotPath));
                Import(content, SnapshotPath);
                Assert.That(content.otherSkills.Length, Is.EqualTo(4));
                Assert.That(content.otherMonsters.Length, Is.EqualTo(2));
                var actual = new JObject
                {
                    ["formatVersion"] = 5,
                    ["monsterSkills"] = JArray.FromObject(content.monsterSkills),
                    ["huntingZones"] = JArray.FromObject(content.huntingZones),
                    ["skills"] = JArray.FromObject(new[] { content.otherSkills[0], content.ring, content.otherSkills[1], content.otherSkills[2], content.otherSkills[3] }),
                    ["monsters"] = JArray.FromObject(new[] { content.monster0, content.otherMonsters[0], content.otherMonsters[1] }),
                    ["growth"] = JObject.FromObject(content.growth),
                    ["parry"] = JObject.FromObject(content.parry),
                    ["items"] = JArray.FromObject(content.items)
                };
                expected["skills"] = JArray.FromObject(expected["skills"].ToObject<ServerContent.RingSkill[]>());
                expected["monsterSkills"] = JArray.FromObject(expected["monsterSkills"].ToObject<ServerContent.RingSkill[]>());
                expected["huntingZones"] = JArray.FromObject(expected["huntingZones"].ToObject<ServerContent.HuntingZone[]>());
                Assert.That(content.monsterSkills[1].phases[1].attack, Is.EqualTo(15));
                Assert.That(content.ring.phases[1].attack, Is.EqualTo(120));
                Assert.That(content.GetHuntingZone(2).regularHp, Is.EqualTo(900));
                Assert.That(content.GetHuntingZone(6).bossHp, Is.EqualTo(36000));
                Assert.That(content.GetHuntingZone(1).minLevel, Is.Zero);
                Assert.That(content.GetHuntingZone(0), Is.Null);
                Assert.That(content.items.Length, Is.EqualTo(9));
                Assert.That(content.GetItem(1).hpRecovery, Is.EqualTo(1500));
                Assert.That(content.GetItem(2).mpRecovery, Is.EqualTo(500));
                Assert.That(content.GetItem(3).name, Is.EqualTo(expected["items"][2]["name"].Value<string>()));
                Assert.That(content.GetItem(3).itemType, Is.EqualTo(2));
                Assert.That(content.GetItem(3).weaponAttackBonus, Is.EqualTo(2));
                Assert.That(content.GetItem(4).defensePercent, Is.EqualTo(2));
                Assert.That(content.GetItem(5).defensePercent, Is.EqualTo(3));
                Assert.That(content.GetItem(6).hpRecovery, Is.EqualTo(3000));
                Assert.That(content.GetItem(7).mpRecovery, Is.EqualTo(1000));
                Assert.That(content.GetItem(8).hpRecovery, Is.EqualTo(4500));
                Assert.That(content.GetItem(9).mpRecovery, Is.EqualTo(1500));
                foreach (uint id in new uint[] { 1, 2, 6, 7, 8, 9 })
                    Assert.That(content.GetItem(id).cooldownTicks, Is.EqualTo(120));
                Assert.That(content.GetItem(33), Is.Null);
                Assert.That(ItemPresentation.Name(3, content), Is.EqualTo(content.GetItem(3).name));
                Assert.That(ItemPresentation.Name(33, content), Is.EqualTo("Item #33"));
                StringAssert.Contains("1500 HP", ItemPresentation.UseDescription(1, content));
                StringAssert.Contains("500 MP", ItemPresentation.UseDescription(2, content));
                expected["monsters"] = JArray.FromObject(expected["monsters"].ToObject<ServerContent.Monster[]>());
                Assert.That(JToken.DeepEquals(actual, expected), Is.True, "Every imported server field must match the JSON snapshot at runtime float precision.");
                Assert.That(content.growth.levels.Length, Is.EqualTo(content.growth.maxLevel));
                string before = EditorJsonUtility.ToJson(content);
                byte[] persisted = File.ReadAllBytes(assetPath);
                Import(content, SnapshotPath);
                Assert.That(EditorJsonUtility.ToJson(content), Is.EqualTo(before));
                Assert.That(File.ReadAllBytes(assetPath), Is.EqualTo(persisted));
                Assert.That(AssetDatabase.AssetPathToGUID(assetPath), Is.EqualTo(guid));
                Assert.That(File.ReadAllBytes(assetPath + ".meta"), Is.EqualTo(metadata));
                Assert.That(content.name, Is.EqualTo("Preserved metadata"));
                string extendedPath = Path.Combine(Path.GetTempPath(), "ContentImport-" + Guid.NewGuid().ToString("N") + ".json");
                try
                {
                    var extended = JObject.Parse(File.ReadAllText(SnapshotPath));
                    var extra = (JObject)extended["items"][2].DeepClone();
                    extra["itemID"] = 100;
                    extra["name"] = "Server supplied item";
                    ((JArray)extended["items"]).Insert(0, extra);
                    File.WriteAllText(extendedPath, extended.ToString());
                    Import(content, extendedPath);
                    Assert.That(content.GetItem(100).name, Is.EqualTo("Server supplied item"));
                    Assert.That(ItemPresentation.Name(100, content), Is.EqualTo("Server supplied item"));
                    Assert.That(ItemPresentation.Icon(100), Is.Null);
                }
                finally { if (File.Exists(extendedPath)) File.Delete(extendedPath); }
            }
            finally
            {
                AssetDatabase.DeleteAsset(assetPath);
                if (content != null) UnityEngine.Object.DestroyImmediate(content);
            }
        }

        [Test]
        [Category("ContentImportDirection")]
        public void InvalidSnapshotsPreserveEntirePriorAssetAndMetadata()
        {
            string assetPath = "Assets/Client/Content/ContentImportTest-" + Guid.NewGuid().ToString("N") + ".asset";
            string invalidPath = Path.Combine(Path.GetTempPath(), "ContentImport-" + Guid.NewGuid().ToString("N") + ".json");
            var content = ScriptableObject.CreateInstance<ServerContent>();
            try
            {
                content.name = "Prior asset metadata";
                content.ring.mana = 987;
                content.monster0.maxHp = 7654;
                content.growth = new ServerContent.Growth
                {
                    maxLevel = 2, levels = new[]
                    {
                        new ServerContent.GrowthLevel { level = 1 },
                        new ServerContent.GrowthLevel { level = 2, requiredTotalExp = 321, maxHpIncrease = 123 }
                    }
                };
                AssetDatabase.CreateAsset(content, assetPath);
                AssetDatabase.SaveAssetIfDirty(content);
                // A rejected import must also preserve pre-existing unsaved edits.
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
                var huntingZones = content.huntingZones;
                var items = content.items;
                var valid = JObject.Parse(File.ReadAllText(SnapshotPath));
                Action<JObject>[] invalidate =
                {
                    json => json["formatVersion"] = 1,
                    json => json.Remove("growth"),
                    json => json.Remove("parry"),
                    json => json["formatVersion"] = 3,
                    json => json["formatVersion"] = 4,
                    json => json.Remove("items"),
                    json => ((JArray)json["items"]).Clear(),
                    json => ((JArray)json["items"]).RemoveAt(0),
                    json => json["items"][1]["itemID"] = 1,
                    json => json["items"][1]["name"] = " ",
                    json => ((JObject)json["items"][0]).Remove("name"),
                    json => json["items"][1]["itemType"] = 2,
                    json => json["items"][1]["weight"] = 0,
                    json => json["items"][1]["mpRecovery"] = 0,
                    json => json["items"][1]["hpRecovery"] = 1,
                    json => json["items"][1]["cooldownTicks"] = 0,
                    json => json["items"][1]["cooldownTicks"] = 41,
                    json => json["items"][2]["hpRecovery"] = 10,
                    json => json["items"][2]["weaponAttackBonus"] = 65536,
                    json => json["items"][0]["weaponAttackBonus"] = 2,
                    json => ((JObject)json["items"][3]).Remove("defensePercent"),
                    json => json["items"][3]["defensePercent"] = 100,
                    json => json["items"][0]["defensePercent"] = 1,
                    json => { json["items"][3]["defensePercent"] = 98; json["items"][4]["defensePercent"] = 2; },
                    json => json["parry"]["windowTicks"] = 0,
                    json => json["parry"]["cooldownTicks"] = 0,
                    json => json["parry"]["charges"] = 2,
                    json => json["growth"]["maxLevel"] = 0,
                    json => json["growth"]["maxLevel"] = (int)ushort.MaxValue + 1,
                    json => json["growth"]["levels"][1]["level"] = 1,
                    json => ((JArray)json["growth"]["levels"]).RemoveAt(1),
                    json => ((JObject)json["growth"]["levels"][1]).Remove("maxHpIncrease"),
                    json => json["growth"]["levels"][0]["requiredTotalExp"] = 1,
                    json => json["growth"]["levels"][0]["maxHpIncrease"] = 1,
                    json => json["growth"]["levels"][2]["requiredTotalExp"] = json["growth"]["levels"][1]["requiredTotalExp"].DeepClone(),
                    json => json["growth"]["levels"][1]["requiredTotalExp"] = (long)uint.MaxValue + 1,
                    json => json["growth"]["levels"][1]["maxHpIncrease"] = (long)int.MaxValue + 1,
                    json => json.Remove("monsterSkills"),
                    json => json["monsterSkills"][1]["skillID"] = 0,
                    json => json["monsterSkills"][1]["phases"][1]["range1"] = -1,
                    json => json.Remove("huntingZones"),
                    json => json["huntingZones"][1]["zoneID"] = 1,
                    json => json["huntingZones"][1]["label"] = " ",
                    json => json["huntingZones"][1]["minLevel"] = 0,
                    json => json["huntingZones"][1]["maxLevel"] = 0,
                    json => json["huntingZones"][1]["regularHp"] = (long)int.MaxValue + 1,
                    json => json["huntingZones"][1]["strongHp"] = 0,
                    json => json["huntingZones"][1]["bossHp"] = 1,
                    json => json["huntingZones"][3]["bossAttack"] = 0,
                    json => json["huntingZones"][0]["regularExp"] = 1,
                    json => json.Remove("skills"),
                    json => ((JObject)json["skills"][0]).Remove("mana"),
                    json => json["skills"][1]["skillID"] = 0,
                    json => json["monsters"][1]["id"] = 0,
                    json => json["skills"][1]["phases"][1]["range1"] = -1,
                    json => json["skills"][1]["phases"][1]["range1"] = double.NaN,
                    json => json["skills"][1]["phases"][1]["action"] = 0,
                    json => json["monsters"][0]["respawnTicks"] = 0,
                    json => json["monsters"][0]["maxHp"] = (long)int.MaxValue + 1,
                    json => json["monsters"][0]["phases"][0]["steps"][0]["skillID"] = 5,
                    json => json["monsters"][0]["phases"][0]["steps"] = new JArray(),
                    json => { json["monsters"][0]["maxHp"] = 1500; json["monsters"][0]["phases"] = new JArray(
                        new JObject { ["hpThreshold"] = 0.5005f, ["steps"] = valid["monsters"][0]["phases"][0]["steps"].DeepClone() },
                        new JObject { ["hpThreshold"] = 0.5004f, ["steps"] = valid["monsters"][0]["phases"][0]["steps"].DeepClone() },
                        new JObject { ["hpThreshold"] = 0.1f, ["steps"] = valid["monsters"][0]["phases"][0]["steps"].DeepClone() }); }
                };
                foreach (var mutation in invalidate)
                {
                    var json = (JObject)valid.DeepClone();
                    mutation(json);
                    File.WriteAllText(invalidPath, json.ToString());
                    var failure = Assert.Throws<TargetInvocationException>(() => Import(content, invalidPath));
                    Assert.That(failure.InnerException, Is.TypeOf<InvalidOperationException>());
                    Assert.That(EditorJsonUtility.ToJson(content), Is.EqualTo(before));
                    Assert.That(content.ring, Is.SameAs(ring));
                    Assert.That(content.monster0, Is.SameAs(monster));
                    Assert.That(content.otherSkills, Is.SameAs(skills));
                    Assert.That(content.otherMonsters, Is.SameAs(monsters));
                    Assert.That(content.growth, Is.SameAs(growth));
                    Assert.That(content.monsterSkills, Is.SameAs(monsterSkills));
                    Assert.That(content.huntingZones, Is.SameAs(huntingZones));
                    Assert.That(content.items, Is.SameAs(items));
                    Assert.That(EditorUtility.IsDirty(content), Is.True);
                    Assert.That(File.ReadAllBytes(assetPath), Is.EqualTo(persisted));
                    Assert.That(File.ReadAllBytes(assetPath + ".meta"), Is.EqualTo(metadata));
                    Assert.That(AssetDatabase.AssetPathToGUID(assetPath), Is.EqualTo(guid));
                }
            }
            finally
            {
                if (File.Exists(invalidPath)) File.Delete(invalidPath);
                AssetDatabase.DeleteAsset(assetPath);
                if (content != null) UnityEngine.Object.DestroyImmediate(content);
            }
        }

        private static string SnapshotPath => Path.Combine(Application.dataPath, "..", "Exports", "ClientData", "ServerContent.json");

        private static void Import(ServerContent content, string path)
        {
            var importer = Type.GetType("Mmorpg.Client.Editor.ServerContentImport, Mmorpg.Client.Editor", true);
            importer.GetMethod("Import", new[] { typeof(ServerContent), typeof(string) }).Invoke(null, new object[] { content, path });
        }
    }
}
#endif
