using System;
using UnityEngine;

namespace Mmorpg.Client
{
    public enum SkillRange : byte { None, Circle, Rectangle, Boss1_1, Boss1_2, Boss2 }
    public enum SkillAction : byte { Charging, Hit }

    // Client data imported from the server-owned static content tables.
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C14ServerContent")]
    public sealed class ServerContent : ScriptableObject
    {
        public Growth growth = new Growth();
        public RingSkill ring = new RingSkill();
        public Monster monster0 = new Monster();
        public RingSkill[] otherSkills =
        {
            new RingSkill { skillID = 0, flags = 0, mana = 1, cooldownTicks = 20, phases = new[]
            {
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 6, action = SkillAction.Hit, range = SkillRange.Circle, range1 = 2, attack = 5 }
            }},
            new RingSkill { skillID = 2, flags = 2, mana = 20, cooldownTicks = 80, phases = new[]
            {
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 10, action = SkillAction.Hit, range = SkillRange.Rectangle, range1 = 12, range2 = 3, attack = 100 }
            }},
            new RingSkill { skillID = 3, flags = 2, mana = 0, cooldownTicks = 0, phases = new[]
            {
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 20, action = SkillAction.Hit, range = SkillRange.Boss1_1, range1 = 10, range2 = 30, attack = 100 },
                new SkillPhase { waitTicks = 10, action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 20, action = SkillAction.Hit, range = SkillRange.Boss1_2, range1 = 10, range2 = 30, attack = 100 }
            }},
            new RingSkill { skillID = 4, flags = 2, mana = 0, cooldownTicks = 0, phases = new[]
            {
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 20, action = SkillAction.Hit, range = SkillRange.Boss2, range1 = 5, attack = 100 },
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 20, action = SkillAction.Hit, range = SkillRange.Boss2, range1 = 10, range2 = 5, attack = 100 },
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 20, action = SkillAction.Hit, range = SkillRange.Boss2, range1 = 15, range2 = 10, attack = 100 },
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 20, action = SkillAction.Hit, range = SkillRange.Boss2, range1 = 20, range2 = 15, attack = 100 },
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 20, action = SkillAction.Hit, range = SkillRange.Boss2, range1 = 25, range2 = 20, attack = 100 }
            }}
        };
        public Monster[] otherMonsters =
        {
            new Monster { id = 1, maxHp = 3000, attack = 8, respawnTicks = 500, moveSpeed = 1.2f, rewardExp = 30,
                phases = new[] { new MonsterPhase { hpThreshold = 1, steps = new[] { new SkillStep { skillID = 0 }, new SkillStep { skillID = 0 }, new SkillStep { skillID = 1 }, new SkillStep { skillID = 0 } } } } },
            new Monster { id = 2, maxHp = 10000, attack = 15, respawnTicks = 6000, moveSpeed = 0.6f, rewardExp = 300,
                phases = new[]
                {
                    new MonsterPhase { hpThreshold = 1, steps = new[] { new SkillStep { skillID = 3, delayTicks = 100 }, new SkillStep { skillID = 4, delayTicks = 100 } } },
                    new MonsterPhase { hpThreshold = 0.5f, steps = new[] { new SkillStep { skillID = 3, delayTicks = 50 }, new SkillStep { skillID = 4, delayTicks = 50 }, new SkillStep { skillID = 3, delayTicks = 50 }, new SkillStep { skillID = 4, delayTicks = 50 } } },
                    new MonsterPhase { hpThreshold = 0.2f, steps = new[] { new SkillStep { skillID = 4, delayTicks = 25 }, new SkillStep { skillID = 3, delayTicks = 25 }, new SkillStep { skillID = 4, delayTicks = 25 }, new SkillStep { skillID = 3, delayTicks = 25 } } }
                } }
        };

        [Serializable]
        public sealed class Growth
        {
            public int maxLevel;
            public GrowthLevel[] levels = Array.Empty<GrowthLevel>();
        }

        [Serializable]
        public sealed class GrowthLevel
        {
            public int level;
            public uint requiredTotalExp;
            public uint maxHpIncrease;
        }

        [Serializable]
        public sealed class RingSkill
        {
            public int skillID = 1;
            public int flags = 2;
            public uint mana = 20;
            public uint cooldownTicks = 120;
            public SkillPhase[] phases =
            {
                new SkillPhase { action = SkillAction.Charging, range = SkillRange.None },
                new SkillPhase { waitTicks = 16, action = SkillAction.Hit, range = SkillRange.Circle, range1 = 8, attack = 100 }
            };
        }

        [Serializable]
        public sealed class SkillPhase
        {
            public uint waitTicks;
            public SkillAction action;
            public SkillRange range;
            public float range1;
            public float range2;
            public uint attack;
        }

        [Serializable]
        public sealed class Monster
        {
            public int id;
            public uint maxHp = 1500;
            public uint attack = 5;
            public uint respawnTicks = 300;
            public float moveSpeed = 1;
            public uint rewardExp = 10;
            public MonsterPhase[] phases = { new MonsterPhase() };
        }

        [Serializable]
        public sealed class MonsterPhase
        {
            public float hpThreshold = 1;
            public SkillStep[] steps = { new SkillStep(), new SkillStep() };
        }

        [Serializable]
        public sealed class SkillStep
        {
            public int skillID;
            public uint delayTicks = 20;
        }
    }
}
