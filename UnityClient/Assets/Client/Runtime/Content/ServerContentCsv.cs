using System;
using System.Collections.Generic;
using System.IO;

namespace Mmorpg.Client
{
    internal static class ServerContentCsv
    {
        internal static void Load(ServerContent target, string directory)
        {
            if (target == null) throw new ArgumentNullException(nameof(target));
            string root = Path.GetFullPath(directory);
            var skills = Skills(Path.Combine(root, "Skills"), "Player");
            if (skills.Length < 2) throw new InvalidDataException("The client requires player skills 0 and 1.");
            var monsterSkills = Skills(Path.Combine(root, "Skills"), "Monster");
            var growth = Growth(Path.Combine(root, "Growth", "Growth.csv"));
            var items = Items(Path.Combine(root, "Items"));
            string monsterDirectory = Path.Combine(root, "Monsters");
            var monsters = Monsters(monsterDirectory, monsterSkills.Length);
            var zones = Zones(Path.Combine(monsterDirectory, "HuntingZones.csv"), growth.maxLevel);
            var species = Species(monsterDirectory, zones, monsters, monsterSkills.Length);
            var otherSkills = new ServerContent.RingSkill[skills.Length - 1];
            int next = 0;
            for (int i = 0; i < skills.Length; i++) if (i != 1) otherSkills[next++] = skills[i];

            // Publish only after every file and cross-reference has been validated.
            target.growth = growth;
            target.items = items;
            target.ring = skills[1];
            target.otherSkills = otherSkills;
            target.monsterSkills = monsterSkills;
            target.monster0 = monsters[0];
            target.otherMonsters = new[] { monsters[1], monsters[2] };
            target.huntingZones = zones;
            target.monsterSpecies = species;
        }

        private static ServerContent.RingSkill[] Skills(string directory, string prefix)
        {
            var result = new List<ServerContent.RingSkill>();
            var phases = new List<List<ServerContent.SkillPhase>>();
            using (var csv = new ContentCsvReader(Path.Combine(directory, prefix + "Skills.csv"), "skillID", "flags", "mana", "coolDown"))
            {
                while (csv.Read())
                {
                    int id = csv.Index("skillID");
                    int flags = csv.Index("flags", byte.MaxValue);
                    csv.Require(id == result.Count, "skillID", "skill IDs must be consecutive from zero");
                    csv.Require((flags & ~3) == 0, "flags", "unknown skill flag bits");
                    result.Add(new ServerContent.RingSkill { skillID = id, flags = flags, mana = csv.UInt("mana"), cooldownTicks = csv.UInt("coolDown") });
                    phases.Add(new List<ServerContent.SkillPhase>());
                }
                csv.Require(result.Count > 0, "skillID", "skill catalog cannot be empty");
            }
            using (var csv = new ContentCsvReader(Path.Combine(directory, prefix + "SkillPhases.csv"),
                "skillID", "phaseIndex", "tick", "actionID", "rangeType", "range1", "range2", "attack"))
            {
                while (csv.Read())
                {
                    int id = csv.Index("skillID");
                    csv.Require(id < result.Count, "skillID", "phase references an unknown skill");
                    csv.Require(csv.Index("phaseIndex") == phases[id].Count && phases[id].Count < ushort.MaxValue,
                        "phaseIndex", "phases must be consecutive from zero and fit the runtime counter");
                    int action = csv.Index("actionID", 1);
                    int range = csv.Index("rangeType", 5);
                    float range1 = csv.Float("range1"), range2 = csv.Float("range2");
                    csv.Require(range1 >= 0 && range2 >= 0, "range1/range2", "range dimensions cannot be negative");
                    if (range == 0) csv.Require(range1 == 0 && range2 == 0 && action == 0, "rangeType", "empty ranges must be charging with zero dimensions");
                    else csv.Require(range1 > 0 && (range != 2 && range != 3 && range != 4 || range2 > 0), "range1/range2", "invalid nonempty range dimensions");
                    phases[id].Add(new ServerContent.SkillPhase { waitTicks = csv.UInt("tick"), action = (SkillAction)action,
                        range = (SkillRange)range, range1 = range1, range2 = range2, attack = csv.UInt("attack") });
                }
                for (int i = 0; i < result.Count; i++)
                {
                    csv.Require(phases[i].Count > 0, "skillID", "missing phases for skill " + i);
                    result[i].phases = phases[i].ToArray();
                }
            }
            return result.ToArray();
        }

        private static ServerContent.Growth Growth(string path)
        {
            var levels = new List<ServerContent.GrowthLevel>();
            using (var csv = new ContentCsvReader(path, "level", "requiredTotalExp", "maxHpIncrease"))
            {
                while (csv.Read())
                {
                    var level = new ServerContent.GrowthLevel { level = csv.Index("level", 100),
                        requiredTotalExp = csv.UInt("requiredTotalExp"), maxHpIncrease = csv.UInt("maxHpIncrease", int.MaxValue) };
                    csv.Require(level.level == levels.Count + 1, "level", "levels must be consecutive from 1 through 100");
                    if (levels.Count == 0) csv.Require(level.requiredTotalExp == 0 && level.maxHpIncrease == 0, "level", "level 1 must have zero EXP and HP increase");
                    else csv.Require(level.requiredTotalExp > levels[levels.Count - 1].requiredTotalExp, "requiredTotalExp", "thresholds must strictly increase");
                    levels.Add(level);
                }
                csv.Require(levels.Count == 100, "level", "expected all 100 growth levels");
            }
            return new ServerContent.Growth { maxLevel = 100, levels = levels.ToArray() };
        }

        private static ServerContent.Item[] Items(string directory)
        {
            var items = new Dictionary<uint, ServerContent.Item>();
            byte maxShield = 0, maxArmor = 0;
            foreach (string name in new[] { "Potions", "Weapons", "Shields", "Armors" })
            {
                string[] columns = name == "Potions" ? new[] { "itemID", "name", "weight", "potionType", "recovery", "cooldownTicks" } :
                    new[] { "itemID", "name", "weight", name == "Weapons" ? "attackBonus" : "defensePercent" };
                using (var csv = new ContentCsvReader(Path.Combine(directory, name + ".csv"), columns))
                {
                    while (csv.Read())
                    {
                        uint id = csv.UInt("itemID");
                        csv.Require(id != 0 && !items.ContainsKey(id), "itemID", "item IDs must be nonzero and unique across item files");
                        var item = new ServerContent.Item { itemID = id, name = csv.Name("name"), weight = csv.UInt("weight"),
                            itemType = (ushort)(name == "Potions" ? 1 : name == "Armors" ? 3 : 2),
                            equipmentSlot = name == "Potions" ? -1 : name == "Weapons" ? 0 : name == "Shields" ? 1 : 2 };
                        csv.Require(item.weight > 0, "weight", "item weight must be positive");
                        if (name == "Potions")
                        {
                            int type = csv.Index("potionType", 1);
                            uint recovery = csv.UInt("recovery", int.MaxValue);
                            csv.Require(recovery > 0, "recovery", "recovery must be positive");
                            item.hpRecovery = type == 0 ? recovery : 0;
                            item.mpRecovery = type == 1 ? recovery : 0;
                            item.cooldownTicks = csv.UInt("cooldownTicks");
                            csv.Require(item.cooldownTicks > 0, "cooldownTicks", "cooldown must be positive");
                        }
                        else if (name == "Weapons") item.weaponAttackBonus = (ushort)csv.Index("attackBonus");
                        else
                        {
                            item.defensePercent = (byte)csv.Index("defensePercent", 99);
                            if (name == "Shields") maxShield = Math.Max(maxShield, item.defensePercent);
                            else maxArmor = Math.Max(maxArmor, item.defensePercent);
                        }
                        items.Add(id, item);
                    }
                    csv.Require(maxShield + maxArmor <= 99, "defensePercent", "combined shield and armor defense must not exceed 99 percent");
                }
            }
            if (items.Count == 0) throw new InvalidDataException(directory + ": item definitions cannot be empty");
            var result = new List<ServerContent.Item>(items.Values);
            result.Sort((left, right) => left.itemID.CompareTo(right.itemID));
            return result.ToArray();
        }

        private static ServerContent.Monster[] Monsters(string directory, int skillCount)
        {
            var phases = new Dictionary<int, List<ServerContent.MonsterPhase>>();
            using (var csv = new ContentCsvReader(Path.Combine(directory, "MonsterPhases.csv"), "monsterID", "phaseIndex", "hpThreshold"))
            {
                while (csv.Read())
                {
                    int id = csv.Index("monsterID");
                    if (!phases.TryGetValue(id, out var list)) { list = new List<ServerContent.MonsterPhase>(); phases.Add(id, list); }
                    csv.Require(csv.Index("phaseIndex", byte.MaxValue) == list.Count && list.Count < 8, "phaseIndex", "expected consecutive phases with at most 8 entries");
                    float threshold = csv.Float("hpThreshold");
                    csv.Require(threshold > 0 && threshold <= 1 && (list.Count == 0 || threshold < list[list.Count - 1].hpThreshold), "hpThreshold", "ratios must strictly decrease within (0, 1]");
                    list.Add(new ServerContent.MonsterPhase { hpThreshold = threshold, steps = Array.Empty<ServerContent.SkillStep>() });
                }
            }
            using (var csv = new ContentCsvReader(Path.Combine(directory, "MonsterSkillSteps.csv"), "monsterID", "phaseIndex", "stepIndex", "skillID", "delayTicks"))
            {
                while (csv.Read())
                {
                    int id = csv.Index("monsterID"), phase = csv.Index("phaseIndex", byte.MaxValue);
                    csv.Require(phases.TryGetValue(id, out var list) && phase < list.Count, "monsterID/phaseIndex", "step references an unknown monster phase");
                    var target = list[phase];
                    csv.Require(csv.Index("stepIndex") == target.steps.Length && target.steps.Length < 16, "stepIndex", "expected consecutive steps with at most 16 entries");
                    int skill = csv.Index("skillID");
                    csv.Require(skill < skillCount, "skillID", "unknown monster skill ID");
                    var steps = target.steps;
                    Array.Resize(ref steps, steps.Length + 1);
                    steps[steps.Length - 1] = new ServerContent.SkillStep { skillID = skill, delayTicks = csv.UInt("delayTicks") };
                    target.steps = steps;
                }
            }
            var result = new List<ServerContent.Monster>();
            using (var csv = new ContentCsvReader(Path.Combine(directory, "Monsters.csv"), "monsterID", "maxHp", "attack", "respawn", "aggroTimeOut", "moveSpeed", "chaseDistance", "rewardExp", "rewardGold"))
            {
                while (csv.Read())
                {
                    int id = csv.Index("monsterID");
                    csv.Require(id == result.Count && id <= 2, "monsterID", "expected consecutive archetypes 0, 1, 2");
                    csv.Require(phases.TryGetValue(id, out var list) && list.Count > 0, "monsterID", "missing monster phases");
                    var monster = new ServerContent.Monster { id = id, maxHp = csv.UInt("maxHp", int.MaxValue), attack = csv.UInt("attack"),
                        respawnTicks = csv.UInt("respawn"), moveSpeed = csv.Float("moveSpeed"), rewardExp = csv.UInt("rewardExp"), phases = list.ToArray() };
                    csv.UInt("aggroTimeOut"); csv.Unsigned("rewardGold", ulong.MaxValue);
                    csv.Require(monster.maxHp > 0 && monster.respawnTicks > 0 && monster.moveSpeed > 0 && csv.Float("chaseDistance") > 0,
                        "maxHp/respawn/moveSpeed/chaseDistance", "HP, respawn ticks, speed and chase distance must be positive");
                    ValidatePhases(csv, monster.phases, monster.maxHp, skillCount);
                    result.Add(monster);
                }
                csv.Require(result.Count == 3, "monsterID", "expected all three archetypes");
                foreach (int id in phases.Keys) csv.Require(id < result.Count, "monsterID", "unknown monster phase owner");
            }
            return result.ToArray();
        }

        private static void ValidatePhases(ContentCsvReader csv, ServerContent.MonsterPhase[] phases, uint hp, int skillCount)
        {
            uint previous = uint.MaxValue;
            for (int i = 0; i < phases.Length; i++)
            {
                var phase = phases[i];
                csv.Require(phase.steps.Length > 0 && phase.steps.Length <= 16 && (phase.steps.Length & (phase.steps.Length - 1)) == 0,
                    "stepIndex", "phase step count must be a power of two from 1 through 16");
                if (i + 1 < phases.Length)
                {
                    uint threshold = (uint)((float)hp * phase.hpThreshold);
                    csv.Require(threshold >= 2 && threshold < previous, "hpThreshold", "non-final absolute thresholds must strictly decrease above one HP");
                    previous = threshold;
                }
                foreach (var step in phase.steps) csv.Require(step.skillID < skillCount, "skillID", "unknown monster skill ID");
            }
        }

        private static ServerContent.HuntingZone[] Zones(string path, int maxLevel)
        {
            var zones = new List<ServerContent.HuntingZone>();
            using (var csv = new ContentCsvReader(path, "zoneID", "label", "minLevel", "maxLevel", "regularHp", "strongHp", "bossHp", "regularAttack", "strongAttack", "bossAttack", "regularExp", "strongExp", "bossExp", "regularGold", "strongGold"))
            {
                while (csv.Read())
                {
                    var zone = new ServerContent.HuntingZone { zoneID = csv.Index("zoneID", 6), label = csv.Name("label"), minLevel = csv.Index("minLevel"), maxLevel = csv.Index("maxLevel"),
                        regularHp = csv.UInt("regularHp", int.MaxValue), strongHp = csv.UInt("strongHp", int.MaxValue), bossHp = csv.UInt("bossHp", int.MaxValue),
                        regularAttack = csv.UInt("regularAttack"), strongAttack = csv.UInt("strongAttack"), bossAttack = csv.UInt("bossAttack"), regularExp = csv.UInt("regularExp"),
                        strongExp = csv.UInt("strongExp"), bossExp = csv.UInt("bossExp"), regularGold = csv.Unsigned("regularGold", ulong.MaxValue), strongGold = csv.Unsigned("strongGold", ulong.MaxValue) };
                    csv.Require(zone.zoneID == zones.Count + 1, "zoneID", "zone IDs must be consecutive from 1 through 6");
                    if (zone.zoneID == 1)
                        csv.Require(zone.minLevel == 0 && zone.maxLevel == 0 && zone.regularHp == 0 && zone.strongHp == 0 && zone.bossHp == 0 && zone.regularAttack == 0 && zone.strongAttack == 0 && zone.bossAttack == 0 && zone.regularExp == 0 && zone.strongExp == 0 && zone.bossExp == 0 && zone.regularGold == 0 && zone.strongGold == 0, "zoneID", "town cannot contain hunting stats");
                    else
                        csv.Require(zone.minLevel > 0 && zone.maxLevel >= zone.minLevel && zone.maxLevel <= maxLevel && zone.regularHp > 0 && zone.strongHp > 0 && zone.regularAttack > 0 && zone.strongAttack > 0 &&
                            (zone.bossHp == 0 ? zone.bossAttack == 0 && zone.bossExp == 0 : zone.bossAttack > 0), "zoneID", "invalid hunting level interval or monster stats");
                    zones.Add(zone);
                }
                csv.Require(zones.Count == 6, "zoneID", "expected one row per world zone including town");
            }
            return zones.ToArray();
        }

        private static ServerContent.MonsterSpecies[] Species(string directory, ServerContent.HuntingZone[] zones, ServerContent.Monster[] monsters, int skillCount)
        {
            var patterns = new ServerContent.MonsterPhase[6][][];
            for (int zone = 2; zone <= 6; zone++)
            {
                patterns[zone - 1] = new ServerContent.MonsterPhase[3][];
                for (int type = 0; type < 3; type++)
                {
                    var source = monsters[type].phases;
                    var copy = new ServerContent.MonsterPhase[source.Length];
                    for (int phase = 0; phase < source.Length; phase++)
                    {
                        var steps = new ServerContent.SkillStep[source[phase].steps.Length];
                        for (int step = 0; step < steps.Length; step++) steps[step] = new ServerContent.SkillStep { skillID = source[phase].steps[step].skillID, delayTicks = source[phase].steps[step].delayTicks };
                        copy[phase] = new ServerContent.MonsterPhase { hpThreshold = source[phase].hpThreshold, steps = steps };
                    }
                    patterns[zone - 1][type] = copy;
                }
            }
            using (var csv = new ContentCsvReader(Path.Combine(directory, "HuntingSkillOverrides.csv"), "zoneID", "monsterType", "phaseIndex", "stepIndex", "skillID", "delayTicks"))
            {
                var seen = new HashSet<(int, int, int, int)>();
                while (csv.Read())
                {
                    int zone = csv.Index("zoneID"), type = csv.Index("monsterType"), phase = csv.Index("phaseIndex", byte.MaxValue), step = csv.Index("stepIndex"), skill = csv.Index("skillID");
                    csv.Require(Available(zones, zone, type), "zoneID/monsterType", "unknown or unavailable hunting monster");
                    var phases = patterns[zone - 1][type];
                    csv.Require(phase < phases.Length && step < phases[phase].steps.Length, "phaseIndex/stepIndex", "unknown monster skill step");
                    csv.Require(seen.Add((zone, type, phase, step)), "stepIndex", "duplicate hunting skill override");
                    csv.Require(skill < skillCount, "skillID", "unknown monster skill ID");
                    phases[phase].steps[step] = new ServerContent.SkillStep { skillID = skill, delayTicks = csv.UInt("delayTicks") };
                }
            }
            var result = new List<ServerContent.MonsterSpecies>();
            var assigned = new HashSet<(int, int)>();
            var identities = new HashSet<(int, int)>();
            using (var csv = new ContentCsvReader(Path.Combine(directory, "HuntingSpecies.csv"), "speciesID", "zoneID", "monsterType", "name"))
            {
                while (csv.Read())
                {
                    int zone = csv.Index("zoneID"), type = csv.Index("monsterType"), id = csv.Index("speciesID");
                    csv.Require(Available(zones, zone, type), "zoneID/monsterType", "unknown or unavailable hunting monster");
                    csv.Require(assigned.Add((zone, type)), "monsterType", "duplicate species for this zone and monster type");
                    csv.Require(identities.Add((zone, id)), "speciesID", "species IDs must be unique within a zone");
                    var source = zones[zone - 1];
                    uint hp = type == 0 ? source.regularHp : type == 1 ? source.strongHp : source.bossHp;
                    ValidatePhases(csv, patterns[zone - 1][type], hp, skillCount);
                    result.Add(new ServerContent.MonsterSpecies { id = id, zoneID = zone, monsterType = type, name = csv.Name("name"),
                        attack = type == 0 ? source.regularAttack : type == 1 ? source.strongAttack : source.bossAttack, phases = patterns[zone - 1][type] });
                }
                for (int zone = 2; zone <= 6; zone++)
                    for (int type = 0; type < 3; type++)
                        csv.Require(!Available(zones, zone, type) || assigned.Contains((zone, type)), "zoneID/monsterType", "missing hunting species");
            }
            return result.ToArray();
        }

        private static bool Available(ServerContent.HuntingZone[] zones, int zone, int type) =>
            zone >= 2 && zone <= zones.Length && type >= 0 && type < 3 && (type != 2 || zones[zone - 1].bossHp != 0);
    }
}
