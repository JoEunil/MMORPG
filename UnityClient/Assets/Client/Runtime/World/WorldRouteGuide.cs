using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace Mmorpg.Client
{
    // Only the nearby part of the route is drawn in the world.
    internal sealed class WorldRouteGuide
    {
        private const float Spacing = 1.5f;
        private const float VisibleDistance = 25f;
        private const float MinimumVisibleDistance = 1.1f;
        private readonly List<Vector3> vertices = new List<Vector3>(144);
        private readonly List<int> triangles = new List<int>(216);
        private readonly GameObject trail;
        private readonly Mesh mesh;
        private readonly Material material;
        private readonly float height;

        internal WorldRouteGuide(Transform parent, float height, Shader shader)
        {
            this.height = height;
            material = new Material(shader) { name = "Walking route guide" };
            material.SetColor("_BaseColor", new Color32(90, 226, 255, 255));
            mesh = new Mesh { name = "Walking route chevrons" };
            mesh.MarkDynamic();
            trail = new GameObject("WalkingRouteTrail", typeof(MeshFilter), typeof(MeshRenderer));
            trail.SetActive(false);
            trail.transform.SetParent(parent, false);
            trail.GetComponent<MeshFilter>().sharedMesh = mesh;
            var renderer = trail.GetComponent<MeshRenderer>();
            renderer.sharedMaterial = material;
            renderer.shadowCastingMode = ShadowCastingMode.Off;
            renderer.receiveShadows = false;
        }

        internal void Show(List<Vector2> route, WorldMap map)
        {
            vertices.Clear();
            triangles.Clear();
            // Anchor spacing at the destination so trimming the travelled prefix does not slide the marks.
            float routeLength = 0;
            for (int i = 1; i < route.Count; i++) routeLength += Vector2.Distance(route[i - 1], route[i]);
            float travelled = 0;
            float nextMark = routeLength - Mathf.Floor((routeLength - MinimumVisibleDistance) / Spacing) * Spacing;
            for (int i = 1; i < route.Count && nextMark <= VisibleDistance; i++)
            {
                Vector2 start = route[i - 1], end = route[i];
                float length = Vector2.Distance(start, end);
                if (length < .001f) continue;
                Vector2 forward = (end - start) / length;
                Vector2 across = new Vector2(-forward.y, forward.x);
                while (nextMark <= travelled + length && nextMark <= VisibleDistance)
                {
                    Vector2 center = start + (nextMark - travelled) * forward;
                    Vector2 tip = center + forward * .28f;
                    Vector2 left = center - forward * .23f + across * .28f;
                    Vector2 right = center - forward * .23f - across * .28f;
                    if (map.CanTraverse(left, tip) && map.CanTraverse(right, tip))
                    {
                        AddArm(left, tip);
                        AddArm(right, tip);
                    }
                    nextMark += Spacing;
                }
                travelled += length;
            }

            mesh.Clear();
            mesh.SetVertices(vertices);
            mesh.SetTriangles(triangles, 0);
            trail.SetActive(vertices.Count > 0);
        }

        private void AddArm(Vector2 start, Vector2 end)
        {
            Vector2 direction = (end - start).normalized;
            Vector2 side = new Vector2(-direction.y, direction.x) * .057f;
            int first = vertices.Count;
            AddVertex(start - side);
            AddVertex(start + side);
            AddVertex(end + side);
            AddVertex(end - side);
            triangles.Add(first);
            triangles.Add(first + 1);
            triangles.Add(first + 2);
            triangles.Add(first);
            triangles.Add(first + 2);
            triangles.Add(first + 3);
        }

        private void AddVertex(Vector2 point)
        {
            var local = trail.transform.InverseTransformPoint(new Vector3(point.x, height, point.y));
            vertices.Add(local);
        }

        internal void Hide()
        {
            if (trail != null) trail.SetActive(false);
        }

        internal void Destroy()
        {
            Object.Destroy(trail);
            Object.Destroy(mesh);
            Object.Destroy(material);
        }
    }
}
