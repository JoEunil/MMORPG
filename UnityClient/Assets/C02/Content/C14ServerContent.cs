using System;
using UnityEngine;

namespace C02
{
    public enum C14SkillRange : byte { None, Circle, Rectangle, Boss1_1, Boss1_2, Boss2 }
    public enum C14SkillAction : byte { Charging, Hit }

    // Only the existing Ring skill and monster 0 are authored by this candidate.
    public sealed class C14ServerContent : ScriptableObject
    {
        public RingSkill ring = new RingSkill();
        public Monster monster0 = new Monster();

        [Serializable]
        public sealed class RingSkill
        {
            public int skillID = 1;
            public int flags = 2;
            public uint mana = 20;
            public uint cooldownTicks = 120;
            public SkillPhase[] phases =
            {
                new SkillPhase { action = C14SkillAction.Charging, range = C14SkillRange.None },
                new SkillPhase { waitTicks = 16, action = C14SkillAction.Hit, range = C14SkillRange.Circle, range1 = 8, attack = 100 }
            };
        }

        [Serializable]
        public sealed class SkillPhase
        {
            public uint waitTicks;
            public C14SkillAction action;
            public C14SkillRange range;
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
