using UnityEngine;

namespace Mmorpg.Client
{
    // Only these explicitly authored bounds/colliders are exported. Decorative foliage is not solid.
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C20MapAuthoring")]
    public sealed class MapAuthoring : MonoBehaviour
    {
        public WorldMap map;
        public BoxCollider ground;
        public BoxCollider[] obstacles;
        [Min(0)] public float actorClearance = .35f;
        [Range(1, 3)] public int zoneColumns = 3;
        [Range(1, 3)] public int zoneRows = 2;
        [Min(1)] public float zoneCoreSize = 200;
        [Min(0)] public float transitionMargin = 5;
        public uint contentVersion = 1;
        [Min(1)] public int previewZoneId = 1;
        public bool showZoneBounds = true;
        public bool showAoiCells = true;
        public bool showTransitions = true;

        private void OnDrawGizmosSelected()
        {
            if (ground == null) return;
            Gizmos.color = new Color(.2f, .8f, .3f, .5f);
            var groundBounds = WorldBounds(ground);
            Gizmos.DrawWireCube(groundBounds.center, groundBounds.size);
            if (obstacles == null) return;
            Gizmos.color = new Color(1, .35f, .15f, .7f);
            foreach (var obstacle in obstacles)
                if (obstacle != null)
                {
                    var bounds = WorldBounds(obstacle);
                    Gizmos.DrawWireCube(bounds.center, bounds.size);
                }
            WorldLayout layout;
            try { layout = WorldLayout.Build(zoneColumns, zoneRows, zoneCoreSize, transitionMargin); }
            catch (System.ArgumentException) { return; }
            foreach (var zone in layout.Zones)
            {
                if (showZoneBounds)
                {
                    Gizmos.color = new Color(.2f, .65f, 1, .8f);
                    DrawRect(zone.ZoneOrigin, zone.LocalBounds, .05f);
                    Gizmos.color = new Color(.3f, 1, .45f, .8f);
                    DrawRect(zone.ZoneOrigin, zone.ExclusiveBounds, .1f);
                }
                if (zone.Id != previewZoneId) continue;
                if (showAoiCells)
                {
                    Gizmos.color = new Color(.9f, .9f, 1, .6f);
                    foreach (var cell in zone.AoiCells) DrawRect(zone.ZoneOrigin, cell, .15f);
                }
                if (showTransitions)
                {
                    Gizmos.color = new Color(1, .65f, .15f, .75f);
                    for (int i = 0; i < zone.NeighborIds.Length; i++)
                        if (zone.NeighborIds[i] != 0) DrawRect(zone.ZoneOrigin, zone.NeighborIntersections[i], .2f);
                }
                Gizmos.color = Color.red;
                Gizmos.DrawSphere(new Vector3(zone.ZoneOrigin.x, .25f, zone.ZoneOrigin.y), .7f);
            }
        }

        private static void DrawRect(Vector2 origin, Rect rect, float height)
        {
            Gizmos.DrawWireCube(new Vector3(origin.x + rect.center.x, height, origin.y + rect.center.y),
                new Vector3(rect.width, 0, rect.height));
        }

        // BoxCollider.bounds is empty when the login screen keeps the world inactive.
        public static Bounds WorldBounds(BoxCollider collider)
        {
            Vector3 half = collider.size * .5f;
            var bounds = new Bounds(collider.transform.TransformPoint(collider.center - half), Vector3.zero);
            for (int corner = 0; corner < 8; corner++)
                bounds.Encapsulate(collider.transform.TransformPoint(collider.center + new Vector3(
                    (corner & 1) == 0 ? -half.x : half.x,
                    (corner & 2) == 0 ? -half.y : half.y,
                    (corner & 4) == 0 ? -half.z : half.z)));
            return bounds;
        }
    }
}
