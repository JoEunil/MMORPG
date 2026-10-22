using System;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace C02.Editor
{
    public static class C15MonsterSpawnExport
    {
        public const string AssetPath = "Assets/C02/Content/C15MonsterSpawns.asset";
        private const int ZoneCount = 6;
        public const int TownZoneId = 1;
        private const int ZoneSpawnBudget = 1000;
        private static readonly UTF8Encoding OutputEncoding = new UTF8Encoding(false);

        [MenuItem("C02/C15/Create Default Monster Spawn Candidate")]
        public static void CreateDefaultCandidate()
        {
            if (File.Exists(Path.Combine(Application.dataPath, "..", AssetPath)) || AssetDatabase.LoadMainAssetAtPath(AssetPath) != null)
                throw new InvalidOperationException("The C15 monster spawn candidate already exists; it was not overwritten.");
            if (!AssetDatabase.IsValidFolder("Assets/C02/Content"))
                AssetDatabase.CreateFolder("Assets/C02", "Content");
            var content = ScriptableObject.CreateInstance<C15MonsterSpawns>();
            AssetDatabase.CreateAsset(content, AssetPath);
            AssetDatabase.SaveAssets();
            Selection.activeObject = content;
        }

        [MenuItem("C02/C15/Export Validated Monster Spawns")]
        public static void ExportValidated()
        {
            var content = AssetDatabase.LoadAssetAtPath<C15MonsterSpawns>(AssetPath);
            ExportValidated(content, Path.Combine(Application.dataPath, "..", "Exports", "ServerData"));
        }

        [MenuItem("C02/C15/Apply Starter Hunt Layout")]
        public static void ApplyStarterHuntLayout()
        {
            var content = AssetDatabase.LoadAssetAtPath<C15MonsterSpawns>(AssetPath);
            if (content == null) throw new InvalidOperationException("Create the existing C15 monster spawn asset first.");

            Undo.RecordObject(content, "Apply starter hunt layout");
            content.regularByCell = new[]
            {
                0, 0, 2, 3, 4,
                0, 2, 3, 4, 5,
                2, 3, 0, 5, 6,
                3, 4, 5, 6, 7,
                4, 5, 6, 7, 8,
            };
            content.strongByCell = new[]
            {
                0, 0, 0, 0, 1,
                0, 0, 0, 1, 1,
                0, 0, 0, 1, 1,
                0, 1, 1, 1, 2,
                1, 1, 1, 2, 2,
            };
            Validate(content);
            EditorUtility.SetDirty(content);
            AssetDatabase.SaveAssetIfDirty(content);
            Selection.activeObject = content;
        }

        public static void ExportValidated(C15MonsterSpawns content, string outputDirectory)
        {
            Validate(content);
            string output = Initializer(content);
            Directory.CreateDirectory(outputDirectory);
            string destination = Path.Combine(outputDirectory, "MonsterSpawns.inc");
            string temporary = Path.Combine(outputDirectory, ".MonsterSpawns-" + Guid.NewGuid().ToString("N") + ".tmp");
            try
            {
                File.WriteAllText(temporary, output, OutputEncoding);
                if (File.Exists(destination)) File.Replace(temporary, destination, null);
                else File.Move(temporary, destination);
            }
            catch (Exception exportFailure)
            {
                try
                {
                    if (File.Exists(temporary)) File.Delete(temporary);
                }
                catch (Exception cleanupFailure)
                {
                    throw new AggregateException("The MonsterSpawns export failed and its temporary file could not be removed.", exportFailure, cleanupFailure);
                }
                throw;
            }
        }

        public static void Validate(C15MonsterSpawns content)
        {
            if (content == null)
                throw new InvalidOperationException("Monster spawn content is required.");
            if (content.regularPerCell < 0 || content.strongPerCell < 0)
                throw new InvalidOperationException("Monster counts per cell cannot be negative.");
            if (content.edgeInset < 1 || content.edgeInset > 41)
                throw new InvalidOperationException("The spawn edge inset must be between 1 and 41.");

            int[] regularCounts = EffectiveCounts(content.regularByCell, content.regularPerCell, "regular");
            int[] strongCounts = EffectiveCounts(content.strongByCell, content.strongPerCell, "strong");
            int[] densities = EffectiveZoneDensity(content.zoneDensityPercent);
            long spawnBudget = 1;
            for (int i = 0; i < regularCounts.Length; i++)
                spawnBudget = checked(spawnBudget + regularCounts[i] + (long)strongCounts[i]);
            if (spawnBudget > ZoneSpawnBudget)
                throw new InvalidOperationException("The authoring budget is 1000 total spawns across the 24 cells and central boss.");

            for (int zone = 0; zone < ZoneCount; zone++)
            {
                long zoneBudget = zone + 1 == TownZoneId ? 0 : 1;
                for (int cell = 0; cell < regularCounts.Length; cell++)
                    zoneBudget = checked(zoneBudget + (long)regularCounts[cell] * densities[zone] / 100 + (long)strongCounts[cell] * densities[zone] / 100);
                if (zoneBudget > ZoneSpawnBudget)
                    throw new InvalidOperationException($"Zone {zone + 1} exceeds the 1000 total spawn budget after applying density.");
            }
        }

        public static int[] EffectiveZoneDensity(int[] configured)
        {
            if (configured == null || configured.Length == 0)
            {
                var defaults = new int[ZoneCount];
                for (int i = 0; i < defaults.Length; i++) defaults[i] = i + 1 == TownZoneId ? 0 : 100;
                return defaults;
            }
            if (configured.Length != ZoneCount)
                throw new InvalidOperationException("Zone density must contain exactly 6 values, one per Zone.");
            var densities = (int[])configured.Clone();
            for (int i = 0; i < densities.Length; i++)
                if (i + 1 == TownZoneId ? densities[i] != 0 : densities[i] < 50 || densities[i] > 200)
                    throw new InvalidOperationException("Town Zone 1 requires zero density; hunting Zones require 50 to 200 percent.");
            return densities;
        }

        public static int[] EffectiveCounts(int[] configured, int fallback, string countName)
        {
            if (configured == null || configured.Length == 0)
            {
                if (fallback < 0)
                    throw new InvalidOperationException("Monster counts per cell cannot be negative.");
                var legacy = new int[25];
                for (int i = 0; i < legacy.Length; i++)
                    if (i != 12) legacy[i] = fallback;
                return legacy;
            }
            if (configured.Length != 25)
                throw new InvalidOperationException($"The custom {countName} counts must contain exactly 25 y-major values.");

            var counts = (int[])configured.Clone();
            for (int i = 0; i < counts.Length; i++)
                if (counts[i] < 0)
                    throw new InvalidOperationException("Monster counts per cell cannot be negative.");
            if (counts[12] != 0)
                throw new InvalidOperationException("The central (2,2) cell is reserved for one boss; regular and strong counts there must be zero.");
            return counts;
        }

        private static string Initializer(C15MonsterSpawns content)
        {
            int[] regularCounts = EffectiveCounts(content.regularByCell, content.regularPerCell, "regular");
            int[] strongCounts = EffectiveCounts(content.strongByCell, content.strongPerCell, "strong");
            int[] densities = EffectiveZoneDensity(content.zoneDensityPercent);
            return string.Format(CultureInfo.InvariantCulture,
                "// Generated by C15MonsterSpawnExport. Do not edit.\n" +
                "namespace Data {{\n" +
                "inline constexpr int REGULAR_MONSTERS_PER_CELL = {0};\n" +
                "inline constexpr int STRONG_MONSTERS_PER_CELL = {1};\n" +
                "inline constexpr int MONSTER_SPAWN_EDGE_INSET = {2};\n" +
                "inline constexpr int MONSTER_TOWN_ZONE_ID = 1;\n" +
                "inline constexpr int REGULAR_MONSTERS_BY_CELL[25] = {{{3}}};\n" +
                "inline constexpr int STRONG_MONSTERS_BY_CELL[25] = {{{4}}};\n" +
                "inline constexpr int MONSTER_ZONE_DENSITY_PERCENT[6] = {{{5}}};\n" +
                "}}\n",
                content.regularPerCell, content.strongPerCell, content.edgeInset,
                FormatCounts(regularCounts), FormatCounts(strongCounts), FormatCounts(densities));
        }

        private static string FormatCounts(int[] counts)
        {
            var values = new StringBuilder();
            for (int i = 0; i < counts.Length; i++)
            {
                if (i > 0) values.Append(", ");
                values.Append(counts[i].ToString(CultureInfo.InvariantCulture));
            }
            return values.ToString();
        }
    }
}
