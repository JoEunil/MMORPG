using UnityEngine;

namespace Mmorpg.Client
{
    [CreateAssetMenu(menuName = "Client/Monster Spawn Content", fileName = "MonsterSpawns")]
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C15MonsterSpawns")]
    public sealed class MonsterSpawns : ScriptableObject
    {
        public int regularPerCell = 8;
        public int strongPerCell = 2;
        public int edgeInset = 1;
        public int[] regularByCell;
        public int[] strongByCell;
        public int[] zoneDensityPercent;
    }
}
