using System;
using System.IO;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using UnityEditor;
using UnityEngine;

namespace Mmorpg.Client.Editor
{
    public static class ServerContentImport
    {
        public const string AssetPath = "Assets/Client/Content/ServerContent.asset";

        [MenuItem("Tools/Client/Content/Import Validated Server Content")]
        public static void ImportValidated()
        {
            Import(AssetDatabase.LoadAssetAtPath<ServerContent>(AssetPath),
                Path.Combine(Application.dataPath, "..", "Exports", "ClientData", "ServerContent.json"));
        }

        public static void Import(ServerContent target, string snapshotPath)
        {
            if (target == null) throw new InvalidOperationException("An existing ServerContent asset is required.");
            JObject snapshot;
            using (var reader = new JsonTextReader(new StringReader(File.ReadAllText(snapshotPath))))
            {
                snapshot = JObject.Load(reader, new JsonLoadSettings { DuplicatePropertyNameHandling = DuplicatePropertyNameHandling.Error });
                if (reader.Read()) throw new InvalidOperationException("Unexpected data after the server content snapshot.");
            }
            if (Integer(snapshot, "formatVersion", 2) != 2)
                throw new InvalidOperationException("Only server content format version 2 is supported.");
            var skills = new ServerContent.RingSkill[5];
            var skillRows = Array(snapshot, "skills");
            if (skillRows.Count != skills.Length) throw new InvalidOperationException("Skill IDs 0 to 4 are required exactly once.");
            foreach (var row in skillRows)
            {
                int id = (int)Integer(row, "skillID", 4);
                if (skills[id] != null) throw new InvalidOperationException("Duplicate skill ID: " + id);
                var phases = Array(row, "phases");
                var skill = new ServerContent.RingSkill
                {
                    skillID = id, flags = (int)Integer(row, "flags", 3), mana = Integer(row, "mana"),
                    cooldownTicks = Integer(row, "cooldownTicks"), phases = new ServerContent.SkillPhase[phases.Count]
                };
                for (int i = 0; i < phases.Count; i++)
                    skill.phases[i] = new ServerContent.SkillPhase
                    {
                        waitTicks = Integer(phases[i], "waitTicks"), action = (SkillAction)Integer(phases[i], "action", 1),
                        range = (SkillRange)Integer(phases[i], "range", 5), range1 = Number(phases[i], "range1"),
                        range2 = Number(phases[i], "range2"), attack = Integer(phases[i], "attack")
                    };
                ValidateSkill(skill);
                skills[id] = skill;
            }
            var monsters = new ServerContent.Monster[3];
            var monsterRows = Array(snapshot, "monsters");
            if (monsterRows.Count != monsters.Length) throw new InvalidOperationException("Monster IDs 0 to 2 are required exactly once.");
            foreach (var row in monsterRows)
            {
                int id = (int)Integer(row, "id", 2);
                if (monsters[id] != null) throw new InvalidOperationException("Duplicate monster ID: " + id);
                var phases = Array(row, "phases");
                var monster = new ServerContent.Monster
                {
                    id = id, maxHp = Integer(row, "maxHp", int.MaxValue), attack = Integer(row, "attack"),
                    respawnTicks = Integer(row, "respawnTicks"), moveSpeed = Number(row, "moveSpeed"),
                    rewardExp = Integer(row, "rewardExp"), phases = new ServerContent.MonsterPhase[phases.Count]
                };
                for (int i = 0; i < phases.Count; i++)
                {
                    var steps = Array(phases[i], "steps");
                    var phase = new ServerContent.MonsterPhase
                    {
                        hpThreshold = Number(phases[i], "hpThreshold"), steps = new ServerContent.SkillStep[steps.Count]
                    };
                    for (int j = 0; j < steps.Count; j++)
                        phase.steps[j] = new ServerContent.SkillStep
                        {
                            skillID = (int)Integer(steps[j], "skillID", 4), delayTicks = Integer(steps[j], "delayTicks")
                        };
                    monster.phases[i] = phase;
                }
                ValidateMonster(monster);
                monsters[id] = monster;
            }

            var growthRow = Field(snapshot, "growth");
            int maxLevel = (int)Integer(growthRow, "maxLevel", ushort.MaxValue);
            var levelRows = Array(growthRow, "levels");
            if (maxLevel == 0 || levelRows.Count != maxLevel)
                throw new InvalidOperationException("Growth requires every level from 1 to a positive maximum level.");
            var growth = new ServerContent.Growth { maxLevel = maxLevel, levels = new ServerContent.GrowthLevel[maxLevel] };
            foreach (var row in levelRows)
            {
                int level = (int)Integer(row, "level", (uint)maxLevel);
                if (level == 0 || growth.levels[level - 1] != null)
                    throw new InvalidOperationException("Growth levels must be present exactly once from 1 to the maximum level.");
                growth.levels[level - 1] = new ServerContent.GrowthLevel
                {
                    level = level, requiredTotalExp = Integer(row, "requiredTotalExp"),
                    maxHpIncrease = Integer(row, "maxHpIncrease", int.MaxValue)
                };
            }
            for (int i = 0; i < growth.levels.Length; i++)
            {
                if (i == 0 && (growth.levels[i].requiredTotalExp != 0 || growth.levels[i].maxHpIncrease != 0))
                    throw new InvalidOperationException("Growth level 1 requires zero total EXP and zero HP increase.");
                if (i > 0 && growth.levels[i].requiredTotalExp <= growth.levels[i - 1].requiredTotalExp)
                    throw new InvalidOperationException("Growth total EXP must strictly increase by level.");
            }

            // All parsing and validation finish before replacing any existing asset fields.
            var oldRing = target.ring;
            var oldMonster = target.monster0;
            var oldSkills = target.otherSkills;
            var oldMonsters = target.otherMonsters;
            var oldGrowth = target.growth;
            bool wasDirty = EditorUtility.IsDirty(target);
            try
            {
                target.ring = skills[1];
                target.otherSkills = new[] { skills[0], skills[2], skills[3], skills[4] };
                target.monster0 = monsters[0];
                target.otherMonsters = new[] { monsters[1], monsters[2] };
                target.growth = growth;
                EditorUtility.SetDirty(target);
                AssetDatabase.SaveAssetIfDirty(target);
            }
            catch
            {
                // Restore the live object if persistence throws. Disk writes are not an atomic transaction.
                target.ring = oldRing;
                target.monster0 = oldMonster;
                target.otherSkills = oldSkills;
                target.otherMonsters = oldMonsters;
                target.growth = oldGrowth;
                if (wasDirty) EditorUtility.SetDirty(target);
                else EditorUtility.ClearDirty(target);
                throw;
            }
        }

        private static JToken Field(JToken row, string name)
        {
            if (!(row is JObject obj) || !obj.TryGetValue(name, out var value) || value.Type == JTokenType.Null)
                throw new InvalidOperationException("Required server content field: " + name);
            return value;
        }

        private static JArray Array(JToken row, string name)
        {
            if (!(Field(row, name) is JArray values)) throw new InvalidOperationException(name + " must be an array.");
            return values;
        }

        private static uint Integer(JToken row, string name, uint maximum = uint.MaxValue)
        {
            var token = Field(row, name);
            if (token.Type != JTokenType.Integer || !decimal.TryParse(token.ToString(), out decimal value) || value < 0 || value > maximum)
                throw new InvalidOperationException(name + " must be an unsigned integer at most " + maximum + ".");
            return (uint)value;
        }

        private static float Number(JToken row, string name)
        {
            var token = Field(row, name);
            if (token.Type != JTokenType.Float && token.Type != JTokenType.Integer)
                throw new InvalidOperationException(name + " must be a finite number.");
            float value = token.Value<float>();
            if (!Finite(value)) throw new InvalidOperationException(name + " must be a finite float.");
            return value;
        }

        private static void ValidateSkill(ServerContent.RingSkill skill)
        {
            int expectedPhases = skill.skillID == 3 ? 4 : skill.skillID == 4 ? 10 : 2;
            if (skill.phases == null || skill.phases.Length != expectedPhases)
                throw new InvalidOperationException("Each existing skill must retain its current phase count.");
            for (int i = 0; i < skill.phases.Length; i++)
            {
                var phase = skill.phases[i];
                SkillAction expectedAction = (i & 1) == 0 ? SkillAction.Charging : SkillAction.Hit;
                if (phase == null || phase.action != expectedAction || !Enum.IsDefined(typeof(SkillRange), phase.range) ||
                    !Finite(phase.range1) || !Finite(phase.range2) || phase.range1 < 0 || phase.range2 < 0)
                    throw new InvalidOperationException("Skill phases must keep the existing Charging/Hit order and finite ranges.");
                if (phase.range == SkillRange.None)
                {
                    if (phase.range1 != 0 || phase.range2 != 0 || phase.action == SkillAction.Hit)
                        throw new InvalidOperationException("An empty range requires zero dimensions and cannot be a Hit phase.");
                }
                else if (phase.range1 <= 0 ||
                    ((phase.range == SkillRange.Rectangle || phase.range == SkillRange.Boss1_1 || phase.range == SkillRange.Boss1_2) && phase.range2 <= 0))
                    throw new InvalidOperationException("The selected skill range requires positive dimensions.");
            }
        }

        private static void ValidateMonster(ServerContent.Monster monster)
        {
            if (monster.maxHp == 0 || monster.maxHp > int.MaxValue || monster.respawnTicks == 0 || !Finite(monster.moveSpeed) || monster.moveSpeed <= 0)
                throw new InvalidOperationException("Monster HP must fit the signed wire value, respawn ticks must be nonzero, and speed must be finite and positive.");
            if (monster.phases == null || monster.phases.Length < 1 || monster.phases.Length > 8)
                throw new InvalidOperationException("Monsters require 1 to 8 phases.");
            float previousThreshold = float.PositiveInfinity;
            uint previousAbsoluteThreshold = uint.MaxValue;
            for (int phaseIndex = 0; phaseIndex < monster.phases.Length; phaseIndex++)
            {
                var phase = monster.phases[phaseIndex];
                if (phase == null || !Finite(phase.hpThreshold) || phase.hpThreshold <= 0 || phase.hpThreshold > 1 || phase.hpThreshold >= previousThreshold)
                    throw new InvalidOperationException("Monster HP thresholds must be finite, positive, at most 1, and strictly decreasing.");
                previousThreshold = phase.hpThreshold;
                // Match uint32_t(maxHp * hpThreshold); HP zero exits before phase transitions.
                uint absoluteThreshold = (uint)((float)monster.maxHp * phase.hpThreshold);
                if (phaseIndex < monster.phases.Length - 1)
                {
                    if (absoluteThreshold < 2 || absoluteThreshold >= previousAbsoluteThreshold)
                        throw new InvalidOperationException("Non-final monster phases require strictly decreasing absolute thresholds above one HP.");
                    previousAbsoluteThreshold = absoluteThreshold;
                }
                if (phase.steps == null || phase.steps.Length < 1 || phase.steps.Length > 16 || (phase.steps.Length & (phase.steps.Length - 1)) != 0)
                    throw new InvalidOperationException("Each monster phase requires a power-of-two step count from 1 to 16.");
                foreach (var step in phase.steps)
                    if (step == null || step.skillID < 0 || step.skillID > 4)
                        throw new InvalidOperationException("Monster skill references must use an existing skill ID from 0 to 4.");
            }
        }

        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
