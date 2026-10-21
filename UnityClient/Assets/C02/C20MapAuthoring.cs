using UnityEngine;

namespace C02
{
    // Only these explicitly authored bounds/colliders are exported. Decorative foliage is not solid.
    public sealed class C20MapAuthoring : MonoBehaviour
    {
        public C20WorldMap map;
        public BoxCollider ground;
        public BoxCollider[] obstacles;
        [Min(0)] public float actorClearance = .35f;

        private void OnDrawGizmosSelected()
        {
            if (ground == null) return;
            Gizmos.color = new Color(.2f, .8f, .3f, .5f);
            Gizmos.DrawWireCube(ground.bounds.center, ground.bounds.size);
            if (obstacles == null) return;
            Gizmos.color = new Color(1, .35f, .15f, .7f);
            foreach (var obstacle in obstacles)
                if (obstacle != null) Gizmos.DrawWireCube(obstacle.bounds.center, obstacle.bounds.size);
        }
    }
}
