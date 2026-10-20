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

        [MenuItem("C02/C14/Export Validated Server Content")]
        public static void ExportValidated()
        {
            var content = AssetDatabase.LoadAssetAtPath<C14ServerContent>(AssetPath);
            ExportValidated(content, Path.Combine(Application.dataPath, "..", "Exports", "ServerData"));
        }

        // Explicit output directory lets the two exporter scenarios use isolated temporary files.
        public static void ExportValidated(C14ServerContent content, string outputDirectory)
        {
            Validate(content);
            string ring = RingInitializer(content.ring);
            string monster = MonsterInitializer(content.monster0);
            C14ExportPair.Write(outputDirectory, "PlayerRing.inc", ring, "Monster0.inc", monster);
        }

        private static void Validate(C14ServerContent content)
        {
            if (content == null || content.ring == null || content.monster0 == null)
                throw new InvalidOperationException("The Ring and monster 0 candidate data are required.");
            var ring = content.ring;
            if (ring.skillID != 1 || ring.flags < 0 || (ring.flags & ~3) != 0)
                throw new InvalidOperationException("Only skill 1 and the existing crit/AOE flag bits are supported.");
            if (ring.phases == null || ring.phases.Length != 2 || ring.phases[0] == null || ring.phases[1] == null ||
                ring.phases[0].action != C14SkillAction.Charging || ring.phases[1].action != C14SkillAction.Hit)
                throw new InvalidOperationException("Ring requires its two phases in Charging, Hit order.");
            foreach (var phase in ring.phases)
            {
                if (!Enum.IsDefined(typeof(C14SkillAction), phase.action) || !Enum.IsDefined(typeof(C14SkillRange), phase.range) ||
                    !Finite(phase.range1) || !Finite(phase.range2) || phase.range1 < 0 || phase.range2 < 0)
                    throw new InvalidOperationException("Ring phase actions and finite range values must match the existing server types.");
                if (phase.range == C14SkillRange.None)
                {
                    if (phase.range1 != 0 || phase.range2 != 0 || phase.action == C14SkillAction.Hit)
                        throw new InvalidOperationException("An empty range requires zero dimensions and cannot be the Hit phase.");
                }
                else if (phase.range1 <= 0 ||
                    ((phase.range == C14SkillRange.Rectangle || phase.range == C14SkillRange.Boss1_1 || phase.range == C14SkillRange.Boss1_2) && phase.range2 <= 0))
                    throw new InvalidOperationException("The selected Ring range requires positive dimensions.");
            }

            var monster = content.monster0;
            if (monster.id != 0 || monster.maxHp == 0 || monster.maxHp > int.MaxValue || monster.respawnTicks == 0 || !Finite(monster.moveSpeed) || monster.moveSpeed <= 0)
                throw new InvalidOperationException("Only monster 0 with positive signed-wire HP, nonzero respawn ticks, and finite positive speed is supported.");
            if (monster.phases == null || monster.phases.Length < 1 || monster.phases.Length > 8)
                throw new InvalidOperationException("Monster 0 requires 1 to 8 phases.");
            float previousThreshold = float.PositiveInfinity;
            for (int phaseIndex = 0; phaseIndex < monster.phases.Length; phaseIndex++)
            {
                var phase = monster.phases[phaseIndex];
                if (phase == null || !Finite(phase.hpThreshold) || phase.hpThreshold <= 0 || phase.hpThreshold > 1 || phase.hpThreshold >= previousThreshold)
                    throw new InvalidOperationException("Monster HP thresholds must be finite, positive, at most 1, and strictly decreasing.");
                previousThreshold = phase.hpThreshold;
                // Match uint32_t(maxHp * hpThreshold); HP zero exits before phase transitions.
                uint absoluteThreshold = (uint)((float)monster.maxHp * phase.hpThreshold);
                if (phaseIndex < monster.phases.Length - 1 && absoluteThreshold < 2)
                    throw new InvalidOperationException("Non-final monster phases require an absolute threshold above one HP.");
                if (phase.steps == null || phase.steps.Length < 1 || phase.steps.Length > 16 || (phase.steps.Length & (phase.steps.Length - 1)) != 0)
                    throw new InvalidOperationException("Each monster phase requires a power-of-two step count from 1 to 16.");
                foreach (var step in phase.steps)
                    if (step == null || step.skillID < 0 || step.skillID > 4)
                        throw new InvalidOperationException("Monster skill references must use an existing skill ID from 0 to 4.");
            }
        }

        private static string RingInitializer(C14ServerContent.RingSkill ring)
        {
            var text = new StringBuilder("// Generated by C14ServerContentExport. Do not edit.\nSkillData{\n");
            text.AppendFormat(CultureInfo.InvariantCulture, "    {0}, {1},\n    {2}, {3},\n    {{\n", ring.skillID, ring.flags, ring.mana, ring.cooldownTicks);
            foreach (var phase in ring.phases)
                text.AppendFormat(CultureInfo.InvariantCulture, "        SkillPhase{{ {0}, {1}, {{SkillRangeType::{2}, {3}, {4}}}, {5} }},\n",
                    phase.waitTicks, (byte)phase.action, phase.range, Float(phase.range1), Float(phase.range2), phase.attack);
            return text.Append("    }\n},\n").ToString();
        }

        private static string MonsterInitializer(C14ServerContent.Monster monster)
        {
            var text = new StringBuilder("// Generated by C14ServerContentExport. Do not edit.\n{\n");
            text.AppendFormat(CultureInfo.InvariantCulture, "    {0},\n    {1},\n    {2},\n    {3},\n    {4},\n    {{\n",
                monster.id, monster.maxHp, monster.attack, monster.respawnTicks, Float(monster.moveSpeed));
            for (int phaseIndex = 0; phaseIndex < monster.phases.Length; phaseIndex++)
            {
                var phase = monster.phases[phaseIndex];
                text.AppendFormat(CultureInfo.InvariantCulture, "        {{ {0}, 0, {1},\n            {{\n", Float(phase.hpThreshold), phase.steps.Length);
                foreach (var step in phase.steps)
                    text.AppendFormat(CultureInfo.InvariantCulture, "                {{ {0}, {1} }},\n", step.skillID, step.delayTicks);
                text.Append("            }\n        },\n");
            }
            text.AppendFormat(CultureInfo.InvariantCulture, "    }},\n    {{{0}}}\n}},\n", monster.rewardExp);
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
