using System;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace C02.Editor
{
    public static class C14ServerContentExport
    {
        public const string AssetPath = "Assets/C02/Content/C14ServerContent.asset";

        [MenuItem("C02/C14/Create Default Server Content Candidate")]
        public static void CreateDefaultCandidate()
        {
            if (File.Exists(Path.Combine(Application.dataPath, "..", AssetPath)) || AssetDatabase.LoadMainAssetAtPath(AssetPath) != null)
                throw new InvalidOperationException("The C14 content candidate already exists; it was not overwritten.");
            if (!AssetDatabase.IsValidFolder("Assets/C02/Content"))
                AssetDatabase.CreateFolder("Assets/C02", "Content");
            var content = ScriptableObject.CreateInstance<C14ServerContent>();
            AssetDatabase.CreateAsset(content, AssetPath);
            AssetDatabase.SaveAssets();
            Selection.activeObject = content;
        }

        [MenuItem("C02/C14/Upgrade Existing Server Content Candidate")]
        public static void UpgradeExistingCandidate()
        {
            var content = AssetDatabase.LoadAssetAtPath<C14ServerContent>(AssetPath);
            if (content == null) throw new InvalidOperationException("The existing C14 content candidate was not found.");
            var defaults = ScriptableObject.CreateInstance<C14ServerContent>();
            if (content.otherSkills == null || content.otherSkills.Length == 0) content.otherSkills = defaults.otherSkills;
            if (content.otherMonsters == null || content.otherMonsters.Length == 0) content.otherMonsters = defaults.otherMonsters;
            EditorUtility.SetDirty(content);
            AssetDatabase.SaveAssets();
            UnityEngine.Object.DestroyImmediate(defaults);
            Selection.activeObject = content;
        }

        [MenuItem("C02/C14/Export Validated Server Content")]
        public static void ExportValidated()
        {
            var content = AssetDatabase.LoadAssetAtPath<C14ServerContent>(AssetPath);
            ExportValidated(content, Path.Combine(Application.dataPath, "..", "Exports", "ServerData"));
        }

        public static void ExportValidated(C14ServerContent content, string outputDirectory)
        {
            Validate(content);
            string skills = SkillsInitializer(content);
            string monsters = MonstersInitializer(content);
            C14ExportPair.Write(outputDirectory, "Skills.inc", skills, "Monsters.inc", monsters);
        }

        private static void Validate(C14ServerContent content)
        {
            if (content == null || content.ring == null || content.monster0 == null)
                throw new InvalidOperationException("The existing Ring and monster 0 candidate data are required.");
            var skills = new C14ServerContent.RingSkill[5];
            skills[1] = content.ring;
            if (content.otherSkills == null || content.otherSkills.Length != 4)
                throw new InvalidOperationException("The existing skill IDs 0, 2, 3, and 4 are required exactly once.");
            foreach (var skill in content.otherSkills)
            {
                if (skill == null || skill.skillID < 0 || skill.skillID >= skills.Length || skill.skillID == 1 || skills[skill.skillID] != null)
                    throw new InvalidOperationException("Skill IDs 0 to 4 must be present exactly once.");
                skills[skill.skillID] = skill;
            }
            for (int id = 0; id < skills.Length; id++)
            {
                if (skills[id] == null || skills[id].skillID != id || skills[id].flags < 0 || (skills[id].flags & ~3) != 0)
                    throw new InvalidOperationException("Skill IDs 0 to 4 and existing crit/AOE flags are required.");
                ValidateSkill(skills[id]);
            }

            var monsters = new C14ServerContent.Monster[3];
            monsters[0] = content.monster0;
            if (content.otherMonsters == null || content.otherMonsters.Length != 2)
                throw new InvalidOperationException("The existing monster IDs 1 and 2 are required exactly once.");
            foreach (var monster in content.otherMonsters)
            {
                if (monster == null || monster.id < 0 || monster.id >= monsters.Length || monster.id == 0 || monsters[monster.id] != null)
                    throw new InvalidOperationException("Monster IDs 0 to 2 must be present exactly once.");
                monsters[monster.id] = monster;
            }
            for (int id = 0; id < monsters.Length; id++)
            {
                if (monsters[id] == null || monsters[id].id != id)
                    throw new InvalidOperationException("Monster IDs 0 to 2 must be present exactly once.");
                ValidateMonster(monsters[id]);
            }
        }

        private static void ValidateSkill(C14ServerContent.RingSkill skill)
        {
            int expectedPhases = skill.skillID == 3 ? 4 : skill.skillID == 4 ? 10 : 2;
            if (skill.phases == null || skill.phases.Length != expectedPhases)
                throw new InvalidOperationException("Each existing skill must retain its current phase count.");
            for (int i = 0; i < skill.phases.Length; i++)
            {
                var phase = skill.phases[i];
                C14SkillAction expectedAction = (i & 1) == 0 ? C14SkillAction.Charging : C14SkillAction.Hit;
                if (phase == null || phase.action != expectedAction || !Enum.IsDefined(typeof(C14SkillRange), phase.range) ||
                    !Finite(phase.range1) || !Finite(phase.range2) || phase.range1 < 0 || phase.range2 < 0)
                    throw new InvalidOperationException("Skill phases must keep the existing Charging/Hit order and finite ranges.");
                if (phase.range == C14SkillRange.None)
                {
                    if (phase.range1 != 0 || phase.range2 != 0 || phase.action == C14SkillAction.Hit)
                        throw new InvalidOperationException("An empty range requires zero dimensions and cannot be a Hit phase.");
                }
                else if (phase.range1 <= 0 ||
                    ((phase.range == C14SkillRange.Rectangle || phase.range == C14SkillRange.Boss1_1 || phase.range == C14SkillRange.Boss1_2) && phase.range2 <= 0))
                    throw new InvalidOperationException("The selected skill range requires positive dimensions.");
            }
        }

        private static void ValidateMonster(C14ServerContent.Monster monster)
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

        private static string SkillsInitializer(C14ServerContent content)
        {
            var skills = new C14ServerContent.RingSkill[5];
            skills[1] = content.ring;
            foreach (var skill in content.otherSkills) skills[skill.skillID] = skill;
            var text = new StringBuilder("// Generated by C14ServerContentExport. Do not edit.\n");
            foreach (var skill in skills)
            {
                text.AppendFormat(CultureInfo.InvariantCulture, "SkillData{{\n    {0}, {1},\n    {2}, {3},\n    {{\n",
                    skill.skillID, skill.flags, skill.mana, skill.cooldownTicks);
                foreach (var phase in skill.phases)
                    text.AppendFormat(CultureInfo.InvariantCulture, "        SkillPhase{{ {0}, {1}, {{SkillRangeType::{2}, {3}, {4}}}, {5} }},\n",
                        phase.waitTicks, (byte)phase.action, phase.range, Float(phase.range1), Float(phase.range2), phase.attack);
                text.Append("    }\n},\n");
            }
            return text.ToString();
        }

        private static string MonstersInitializer(C14ServerContent content)
        {
            var monsters = new C14ServerContent.Monster[3];
            monsters[0] = content.monster0;
            foreach (var monster in content.otherMonsters) monsters[monster.id] = monster;
            var text = new StringBuilder("// Generated by C14ServerContentExport. Do not edit.\n");
            foreach (var monster in monsters)
            {
                text.AppendFormat(CultureInfo.InvariantCulture, "{{\n    {0},\n    {1},\n    {2},\n    {3},\n    {4},\n    {{\n",
                    monster.id, monster.maxHp, monster.attack, monster.respawnTicks, Float(monster.moveSpeed));
                foreach (var phase in monster.phases)
                {
                    text.AppendFormat(CultureInfo.InvariantCulture, "        {{ {0}, 0, {1},\n            {{\n", Float(phase.hpThreshold), phase.steps.Length);
                    foreach (var step in phase.steps)
                        text.AppendFormat(CultureInfo.InvariantCulture, "                {{ {0}, {1} }},\n", step.skillID, step.delayTicks);
                    text.Append("            }\n        },\n");
                }
                text.AppendFormat(CultureInfo.InvariantCulture, "    }},\n    {{{0}}}\n}},\n", monster.rewardExp);
            }
            return text.ToString();
        }

        private static string Float(float value)
        {
            string literal = value.ToString("R", CultureInfo.InvariantCulture);
            if (literal.IndexOf('.') < 0 && literal.IndexOf('E') < 0 && literal.IndexOf('e') < 0) literal += ".0";
            return literal + "f";
        }

        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
