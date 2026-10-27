using System;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;

namespace Mmorpg.Client.Editor
{
    // A single decorative gate approach; it is not a navigation or collision boundary.
    public static class EntryTrailSetup
    {
        private const string MeshPath = "Assets/Client/Art/FieldGeometry/EntryTrailRibbon.asset";
        private const string SourceMaterialPath = "Assets/Client/Art/FieldMaterials/WornTrail.mat";
        private const string MaterialPath = "Assets/Client/Art/FieldGeometry/EntryTrailFeather.mat";
        private const string ObjectName = "EntryTrailRibbon";

        [MenuItem("Tools/Client/Art/Apply Entry Trail Ribbon")]
        public static void Apply()
        {
            var scene = EditorSceneManager.GetActiveScene();
            if (EditorApplication.isPlaying || scene.path != "Assets/Client/Scenes/World.unity" || scene.isDirty)
                throw new InvalidOperationException("Open the saved, clean C02 scene in Edit Mode before applying the entry trail.");
            Transform field = null;
            foreach (var root in scene.GetRootGameObjects())
            {
                foreach (var candidate in root.GetComponentsInChildren<Transform>(true))
                    if (candidate.name == "C02World")
                    {
                        var found = candidate.Find("V04MeanderingMeadow");
                        if (found == null) continue;
                        if (field != null) throw new InvalidOperationException("Expected one V04 field in C02.");
                        field = found;
                    }
            }
            if (field == null) throw new InvalidOperationException("The existing V04 field is required; this tool does not rebuild it.");
            Undo.IncrementCurrentGroup();
            int group = Undo.GetCurrentGroup();
            Undo.SetCurrentGroupName("Apply Entry Trail Ribbon");
            bool changed;
            try { changed = ApplyToField(field, true); }
            finally { Undo.CollapseUndoOperations(group); }
            if (changed)
            {
                Undo.FlushUndoRecordObjects();
                EditorSceneManager.MarkSceneDirty(scene);
                if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Entry trail scene save failed.");
            }
            Debug.Log("Entry trail ribbon: " + (changed ? "applied and scene saved" : "already matches; scene unchanged") + ". No colliders or ground texture changes.");
        }

        // Shared with full V04 authoring so a later rebuild retains the same small approach.
        public static bool ApplyToField(Transform field, bool recordUndo)
        {
            if (EditorApplication.isPlaying || field == null || field.name != "V04MeanderingMeadow")
                throw new InvalidOperationException("Entry trail requires the V04 field in Edit Mode.");
            var source = AssetDatabase.LoadAssetAtPath<Material>(SourceMaterialPath);
            if (source == null || source.mainTexture == null ||
                (source.mainTextureScale - new Vector2(.14f, .7f)).sqrMagnitude > .000001f ||
                (source.mainTextureOffset - new Vector2(.43f, .15f)).sqrMagnitude > .000001f)
                throw new InvalidOperationException("Expected the existing WornTrail earth-strip material and UV transform; no material is rewritten.");
            var shader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/Client/Art/FieldGeometry/EntryTrailFeather.shader");
            if (shader == null || !shader.isSupported) throw new InvalidOperationException("The entry trail feather shader is required.");
            var material = AssetDatabase.LoadAssetAtPath<Material>(MaterialPath);
            var tint = new Color(1.12f, 1.13f, 1.17f, 1);
            if (material != null && (material.shader != shader || material.GetTexture("_BaseMap") != source.mainTexture ||
                material.GetColor("_BaseColor") != tint || material.GetTextureScale("_BaseMap") != source.mainTextureScale ||
                material.GetTextureOffset("_BaseMap") != source.mainTextureOffset))
                throw new InvalidOperationException("The existing feather material differs from this candidate; review it explicitly before applying.");
            var existing = field.Find(ObjectName);
            int matchingChildren = 0;
            foreach (Transform child in field) if (child.name == ObjectName) matchingChildren++;
            if (matchingChildren > 1) throw new InvalidOperationException("Duplicate entry trails require review before applying.");
            if (existing != null && (existing.localPosition != Vector3.zero || existing.localRotation != Quaternion.identity ||
                existing.localScale != Vector3.one || existing.GetComponent<MeshFilter>() == null ||
                existing.GetComponent<MeshRenderer>() == null || existing.GetComponent<Collider>() != null ||
                existing.childCount != 0 || !existing.gameObject.activeSelf || !existing.GetComponent<MeshRenderer>().enabled))
                throw new InvalidOperationException("The existing entry trail was edited. Review it before applying; its transform/components will not be replaced.");

            const int sections = 29;
            const int columns = 5;
            var vertices = new Vector3[sections * columns];
            var uv = new Vector2[vertices.Length];
            var colors = new Color[vertices.Length];
            var triangles = new int[(sections - 1) * (columns - 1) * 6];
            var lateral = new[] { -1f, -.55f, 0f, .55f, 1f };
            var opacity = new[] { 0f, .55f, .65f, .55f, 0f };
            Vector2 a = new Vector2(10, -15), b = new Vector2(7, 9), c = new Vector2(10, 26), d = new Vector2(28, 32);
            Vector2 Point(float t)
            {
                float u = 1 - t;
                return u*u*u*a + 3*u*u*t*b + 3*u*t*t*c + t*t*t*d;
            }
            // The first V04 cubic is monotone in Y. Clip its centreline to local Y 4..32.
            float low = 0, high = 1;
            for (int i = 0; i < 24; i++)
            {
                float middle = (low + high) * .5f;
                if (Point(middle).y < 4) low = middle; else high = middle;
            }
            float start = high;
            for (int i = 0; i < sections; i++)
            {
                float along = i / (float)(sections - 1), t = Mathf.Lerp(start, 1, along), u = 1 - t;
                Vector2 centre = Point(t);
                Vector2 tangent = (3*u*u*(b-a) + 6*u*t*(c-b) + 3*t*t*(d-c)).normalized;
                Vector2 right = new Vector2(tangent.y, -tangent.x);
                // Nominal 3.2m width, with small unequal worn edges rather than tiled slabs.
                float leftWidth = 1.6f * (.94f + .06f * Mathf.Sin(i * 1.73f + .4f));
                float rightWidth = 1.6f * (.94f + .06f * Mathf.Sin(i * 1.31f + 2.1f));
                float endFade = Mathf.SmoothStep(0, 1, Mathf.Clamp01(along / .12f)) *
                    Mathf.SmoothStep(0, 1, Mathf.Clamp01((1 - along) / .12f));
                for (int column = 0; column < columns; column++)
                {
                    float across = lateral[column];
                    Vector2 point = centre + right * across * (across < 0 ? leftWidth : rightWidth);
                    int vertex = i * columns + column;
                    vertices[vertex] = new Vector3(point.x, -.015f, Mathf.Clamp(point.y, 4, 32));
                    // WornTrail U*.14+.43 samples only .486..514: inside its earth core.
                    uv[vertex] = new Vector2(Mathf.Lerp(.4f, .6f, (across + 1) * .5f), along);
                    colors[vertex] = new Color(1, 1, 1, opacity[column] * endFade);
                    if (i == sections - 1 || column == columns - 1) continue;
                    int index = (i * (columns - 1) + column) * 6;
                    triangles[index] = vertex; triangles[index+1] = vertex+columns; triangles[index+2] = vertex+1;
                    triangles[index+3] = vertex+1; triangles[index+4] = vertex+columns; triangles[index+5] = vertex+columns+1;
                }
            }
            var mesh = AssetDatabase.LoadAssetAtPath<Mesh>(MeshPath);
            if (mesh != null)
            {
                var oldVertices = mesh.vertices; var oldUv = mesh.uv; var oldTriangles = mesh.triangles; var oldColors = mesh.colors;
                bool same = oldVertices.Length == vertices.Length && oldUv.Length == uv.Length && oldTriangles.Length == triangles.Length && oldColors.Length == colors.Length;
                for (int i = 0; same && i < vertices.Length; i++)
                    same = (oldVertices[i] - vertices[i]).sqrMagnitude < .0000000001f && (oldUv[i] - uv[i]).sqrMagnitude < .0000000001f && oldColors[i] == colors[i];
                for (int i = 0; same && i < triangles.Length; i++) same = oldTriangles[i] == triangles[i];
                if (!same) throw new InvalidOperationException("EntryTrailRibbon.asset differs from this candidate. Review/update the asset explicitly; it was preserved.");
            }
            else
            {
                if (!AssetDatabase.IsValidFolder("Assets/Client/Art/FieldGeometry"))
                    throw new InvalidOperationException("The existing V04 art folder is required.");
                mesh = new Mesh { name = ObjectName, vertices = vertices, uv = uv, colors = colors, triangles = triangles };
                mesh.RecalculateNormals(); mesh.RecalculateBounds();
                AssetDatabase.CreateAsset(mesh, MeshPath);
                AssetDatabase.SaveAssetIfDirty(mesh);
            }
            if (material == null)
            {
                material = new Material(shader) { name = "EntryTrailFeather" };
                material.SetTexture("_BaseMap", source.mainTexture);
                material.SetTextureScale("_BaseMap", source.mainTextureScale);
                material.SetTextureOffset("_BaseMap", source.mainTextureOffset);
                material.SetColor("_BaseColor", tint);
                AssetDatabase.CreateAsset(material, MaterialPath);
                AssetDatabase.SaveAssetIfDirty(material);
            }
            bool changed = existing == null;
            if (existing == null)
            {
                var trail = new GameObject(ObjectName, typeof(MeshFilter), typeof(MeshRenderer));
                if (recordUndo) Undo.RegisterCreatedObjectUndo(trail, "Create entry trail ribbon");
                trail.transform.SetParent(field, false);
                existing = trail.transform;
                trail.GetComponent<MeshRenderer>().shadowCastingMode = ShadowCastingMode.Off;
            }
            var filter = existing.GetComponent<MeshFilter>();
            var renderer = existing.GetComponent<MeshRenderer>();
            if (filter.sharedMesh != mesh)
            {
                if (recordUndo) Undo.RecordObject(filter, "Assign entry trail mesh");
                filter.sharedMesh = mesh; changed = true;
            }
            if (renderer.sharedMaterial != material)
            {
                if (recordUndo) Undo.RecordObject(renderer, "Assign existing trail material");
                renderer.sharedMaterial = material; changed = true;
            }
            return changed;
        }
    }
}
