using System;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace Mmorpg.Client.Editor
{
    public static class MonsterSpawnExport
    {
        public const string AssetPath = "Assets/Client/Content/MonsterSpawns.asset";
        private const int ZoneCount = 6;
        public const int TownZoneId = 1;
        private const int ZoneSpawnBudget = 1000;
        private static readonly UTF8Encoding OutputEncoding = new UTF8Encoding(false);

        [MenuItem("Tools/Client/Spawns/Create Default Monster Spawn Candidate")]
        public static void CreateDefaultCandidate()
        {
            if (File.Exists(Path.Combine(Application.dataPath, "..", AssetPath)) || AssetDatabase.LoadMainAssetAtPath(AssetPath) != null)
                throw new InvalidOperationException("The C15 monster spawn candidate already exists; it was not overwritten.");
            if (!AssetDatabase.IsValidFolder("Assets/Client/Content"))
                AssetDatabase.CreateFolder("Assets/Client", "Content");
            var content = ScriptableObject.CreateInstance<MonsterSpawns>();
            AssetDatabase.CreateAsset(content, AssetPath);
            AssetDatabase.SaveAssets();
            Selection.activeObject = content;
        }

        [MenuItem("Tools/Client/Spawns/Export Validated Monster Spawns")]
        public static void ExportValidated()
        {
            var content = AssetDatabase.LoadAssetAtPath<MonsterSpawns>(AssetPath);
            ExportValidated(content, Path.Combine(Application.dataPath, "..", "Exports", "ServerData"));
        }

        [MenuItem("Tools/Client/Spawns/Apply Starter Hunt Layout")]
        public static void ApplyStarterHuntLayout()
        {
            var content = AssetDatabase.LoadAssetAtPath<MonsterSpawns>(AssetPath);
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

        public static void ExportValidated(MonsterSpawns content, string outputDirectory)
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

        public static void Validate(MonsterSpawns content)
        {
            if (content == null)
                throw new InvalidOperationException("Monster spawn content is required.");
            if (content.regularPerCell < 0 || content.strongPerCell < 0)
                throw new InvalidOperationException("Monster counts per cell cannot be negative.");
            if (content.edgeInset < 1 || content.edgeInset > 41)
                throw new InvalidOperationException("The spawn edge inset must be between 1 and 41.");

            bool totals = UsesTotals(content);
            EffectiveZoneDensity(content.zoneDensityPercent);
            if (!totals)
            {
                int[] regularWeights = EffectiveCounts(content.regularByCell, content.regularPerCell, "regular");
                int[] strongWeights = EffectiveCounts(content.strongByCell, content.strongPerCell, "strong");
                long budget = 1; // The shared legacy layout includes one boss in Zones 4 and 6.
                for (int cell = 0; cell < 25; cell++) budget += (long)regularWeights[cell] + strongWeights[cell];
                if (budget > ZoneSpawnBudget)
                    throw new InvalidOperationException("The authoring budget is 1000 total spawns across the 24 cells and the Zone 4/6 central boss.");
            }
            int[][] regular = EffectiveZoneCounts(content, false);
            int[][] strong = EffectiveZoneCounts(content, true);
            int[] bosses = EffectiveBossCounts(content);
            for (int zone = 0; zone < ZoneCount; zone++)
            {
                long budget = bosses[zone];
                for (int cell = 0; cell < 25; cell++) budget += (long)regular[zone][cell] + strong[zone][cell];
                if (budget > ZoneSpawnBudget)
                    throw new InvalidOperationException($"Zone {zone + 1} exceeds the 1000 total spawn budget.");
            }
        }

        private static bool UsesTotals(MonsterSpawns content)
        {
            bool configured = content.regularTotals != null && content.regularTotals.Length > 0 ||
                content.strongTotals != null && content.strongTotals.Length > 0 ||
                content.bossCounts != null && content.bossCounts.Length > 0;
            if (!configured) return false;
            if (content.regularTotals == null || content.regularTotals.Length != ZoneCount ||
                content.strongTotals == null || content.strongTotals.Length != ZoneCount ||
                content.bossCounts == null || content.bossCounts.Length != ZoneCount)
                throw new InvalidOperationException("Regular totals, strong totals and boss counts must each contain exactly 6 values.");
            for (int zone = 0; zone < ZoneCount; zone++)
            {
                if (content.regularTotals[zone] < 0 || content.strongTotals[zone] < 0 || content.bossCounts[zone] < 0 || content.bossCounts[zone] > 1)
                    throw new InvalidOperationException("Zone totals must be nonnegative; boss counts must be 0 or 1.");
                if (zone != 3 && zone != 5 && content.bossCounts[zone] != 0)
                    throw new InvalidOperationException("Bosses are supported only in Zones 4 and 6.");
                if (zone + 1 == TownZoneId && (content.regularTotals[zone] != 0 || content.strongTotals[zone] != 0 || content.bossCounts[zone] != 0))
                    throw new InvalidOperationException("Town Zone 1 requires zero regular, strong and boss counts.");
            }
            return true;
        }

        public static int[] EffectiveBossCounts(MonsterSpawns content)
        {
            return UsesTotals(content) ? (int[])content.bossCounts.Clone() : new[] { 0, 0, 0, 1, 0, 1 };
        }

        public static int[][] EffectiveZoneCounts(MonsterSpawns content, bool strong)
        {
            bool totals = UsesTotals(content);
            int[] weights = EffectiveCounts(strong ? content.strongByCell : content.regularByCell,
                strong ? content.strongPerCell : content.regularPerCell, strong ? "strong" : "regular");
            int[] densities = totals ? null : EffectiveZoneDensity(content.zoneDensityPercent);
            long weightSum = 0;
            foreach (int weight in weights) weightSum += weight;
            var result = new int[ZoneCount][];
            for (int zone = 0; zone < ZoneCount; zone++)
            {
                result[zone] = new int[25];
                if (!totals)
                {
                    for (int cell = 0; cell < 25; cell++)
                        result[zone][cell] = checked((int)((long)weights[cell] * densities[zone] / 100));
                    continue;
                }
                int total = strong ? content.strongTotals[zone] : content.regularTotals[zone];
                if (total == 0) continue;
                if (weightSum == 0)
                    throw new InvalidOperationException("A positive zone total requires at least one positive cell weight.");
                var remainders = new long[25];
                int assigned = 0;
                for (int cell = 0; cell < 25; cell++)
                {
                    long weighted = (long)total * weights[cell];
                    result[zone][cell] = (int)(weighted / weightSum);
                    remainders[cell] = weighted % weightSum;
                    assigned += result[zone][cell];
                }
                // Largest remainder, with lower y-major cell index winning ties.
                while (assigned < total)
                {
                    int best = 0;
                    for (int cell = 1; cell < 25; cell++)
                        if (remainders[cell] > remainders[best]) best = cell;
                    result[zone][best]++;
                    remainders[best] = -1;
                    assigned++;
                }
            }
            return result;
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
                throw new InvalidOperationException("The central (2,2) cell is reserved for bosses; regular and strong counts there must be zero.");
            return counts;
        }

        private static string Initializer(MonsterSpawns content)
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
                "inline constexpr int MONSTER_REGULAR_COUNTS[6][25] = {{\n{6}\n}};\n" +
                "inline constexpr int MONSTER_STRONG_COUNTS[6][25] = {{\n{7}\n}};\n" +
                "inline constexpr int MONSTER_BOSS_COUNTS[6] = {{{8}}};\n" +
                "}}\n",
                content.regularPerCell, content.strongPerCell, content.edgeInset,
                FormatCounts(regularCounts), FormatCounts(strongCounts), FormatCounts(densities),
                FormatZoneCounts(EffectiveZoneCounts(content, false)), FormatZoneCounts(EffectiveZoneCounts(content, true)),
                FormatCounts(EffectiveBossCounts(content)));
        }

        private static string FormatZoneCounts(int[][] counts)
        {
            var rows = new string[counts.Length];
            for (int zone = 0; zone < counts.Length; zone++) rows[zone] = "    {" + FormatCounts(counts[zone]) + "}";
            return string.Join(",\n", rows);
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
