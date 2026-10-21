using UnityEngine;

namespace C02
{
    [CreateAssetMenu(menuName = "C02/Monster Spawn Content", fileName = "C15MonsterSpawns")]
    public sealed class C15MonsterSpawns : ScriptableObject
    {
        public int regularPerCell = 8;
        public int strongPerCell = 2;
        public int edgeInset = 1;
        public int[] regularByCell;
        public int[] strongByCell;
        public int[] zoneDensityPercent;
    }
}
