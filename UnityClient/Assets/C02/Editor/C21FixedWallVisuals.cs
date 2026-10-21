using System;
using System.Collections.Generic;
using C02;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class C21FixedWallVisuals
{
    private const string RootName = "C20FixedWorldMap";
    private const string AssetFolder = "Assets/C02/Art/FixedWorldWalls";
    private const string TemplatePath = "Assets/C02/Art/V02_WallStone-1_0_0.asset";
    private const string LimestonePath = "Assets/C02/Art/WeatheredLimestone.mat";
    private const string FoundationPath = "Assets/C02/Art/FoundationStone.mat";
    private const float Gap = .03f;
    private const float CoreInset = .1f;
    private const float StoneHeight = .5f;

    [MenuItem("C02/World/Apply Fixed Wall Visuals")]
    public static void Apply()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the saved C02 scene in Edit Mode.");

        var authors = UnityEngine.Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        var author = authors.Length == 1 ? authors[0] : null;
        var root = author == null ? null : author.gameObject;
        if (root == null || authors.Length != 1 || root.scene != scene || root.name != RootName || root.transform.parent == null || root.transform.parent.name != "C02World" ||
            root.transform.position != Vector3.zero || root.transform.rotation != Quaternion.identity || root.transform.localScale != Vector3.one ||
            root.transform.childCount != 19 || author.obstacles == null || author.obstacles.Length != 18)
            throw new InvalidOperationException("The fixed world root and its 18 authored walls do not match the expected scene.");

        var walls = new Dictionary<string, BoxCollider>(StringComparer.Ordinal);
        var expectedNames = new HashSet<string>(StringComparer.Ordinal);
        for (int zone = 1; zone <= 6; zone++)
            for (int wall = 1; wall <= 3; wall++)
                expectedNames.Add("Zone" + zone + "_RuinedWall" + wall);
        foreach (var collider in author.obstacles)
        {
            if (collider == null || collider.transform.parent != root.transform || !expectedNames.Contains(collider.name) ||
                walls.ContainsKey(collider.name) || collider.isTrigger || collider.center != Vector3.zero || collider.size != Vector3.one ||
                collider.transform.localRotation != Quaternion.identity || collider.transform.childCount != 0)
                throw new InvalidOperationException("An authored wall collider differs from the expected unchanged collision box.");
            Vector3 expectedScale = collider.name.EndsWith("_RuinedWall2", StringComparison.Ordinal)
                ? new Vector3(10, 2.7f, 2) : new Vector3(2, 2.7f, 10);
            if ((collider.transform.localScale - expectedScale).sqrMagnitude > .000001f ||
                collider.GetComponent<MeshFilter>() == null || collider.GetComponent<MeshRenderer>() == null)
                throw new InvalidOperationException("An authored wall mesh or collider transform is missing or changed.");
            walls.Add(collider.name, collider);
        }
        if (walls.Count != 18) throw new InvalidOperationException("All 18 authored wall colliders must be present.");
        foreach (Transform child in root.transform)
            if (child.name != "NavigationGround" && !walls.ContainsKey(child.name))
                throw new InvalidOperationException("Unexpected child under the fixed world root: " + child.name);

        var template = AssetDatabase.LoadAssetAtPath<Mesh>(TemplatePath);
        var limestone = AssetDatabase.LoadAssetAtPath<Material>(LimestonePath);
        var foundation = AssetDatabase.LoadAssetAtPath<Material>(FoundationPath);
        if (template == null || template.vertexCount != 24 || template.subMeshCount != 1 ||
            !Near(template.bounds.size, Vector3.one, .04f) || limestone == null || foundation == null)
            throw new InvalidOperationException("The existing cuboid stone mesh and both existing stone materials are required.");

        string xPath = AssetFolder + "/FixedWall_LongX.asset";
        string zPath = AssetFolder + "/FixedWall_LongZ.asset";
        var expectedX = BuildMesh(template, true, "FixedWall_LongX");
        Mesh expectedZ = null;
        Mesh meshX;
        Mesh meshZ;
        try
        {
            expectedZ = BuildMesh(template, false, "FixedWall_LongZ");
            meshX = LoadOrCreate(xPath, expectedX);
            meshZ = LoadOrCreate(zPath, expectedZ);
        }
        finally
        {
            if (expectedX != null && !AssetDatabase.Contains(expectedX)) UnityEngine.Object.DestroyImmediate(expectedX);
            if (expectedZ != null && !AssetDatabase.Contains(expectedZ)) UnityEngine.Object.DestroyImmediate(expectedZ);
        }
        var materials = new[] { limestone, foundation };

        var filters = new List<MeshFilter>(18);
        var renderers = new List<MeshRenderer>(18);
        var meshes = new List<Mesh>(18);
        bool changed = false;
        foreach (var collider in author.obstacles)
        {
            var filter = collider.GetComponent<MeshFilter>();
            var renderer = collider.GetComponent<MeshRenderer>();
            var mesh = collider.name.EndsWith("_RuinedWall2", StringComparison.Ordinal) ? meshX : meshZ;
            filters.Add(filter); renderers.Add(renderer); meshes.Add(mesh);
            var oldMaterials = renderer.sharedMaterials;
            if (filter.sharedMesh != mesh || oldMaterials.Length != 2 || oldMaterials[0] != limestone || oldMaterials[1] != foundation)
                changed = true;
        }
        if (!changed)
        {
            Debug.Log("Fixed wall visuals already match; the scene was not changed.");
            return;
        }

        var undoObjects = new UnityEngine.Object[36];
        for (int i = 0; i < 18; i++) { undoObjects[i] = filters[i]; undoObjects[i + 18] = renderers[i]; }
        Undo.RecordObjects(undoObjects, "Apply Fixed Wall Visuals");
        for (int i = 0; i < 18; i++)
        {
            filters[i].sharedMesh = meshes[i];
            renderers[i].sharedMaterials = materials;
            EditorUtility.SetDirty(filters[i]);
            EditorUtility.SetDirty(renderers[i]);
        }
        Undo.FlushUndoRecordObjects();
        EditorSceneManager.MarkSceneDirty(scene);
        if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Saving the fixed wall visuals failed.");
        Debug.Log("Applied staggered stone visuals to 18 existing walls; collision boxes and navigation data were unchanged.");
    }

    private static Mesh LoadOrCreate(string path, Mesh expected)
    {
        var existing = AssetDatabase.LoadAssetAtPath<Mesh>(path);
        if (existing != null)
        {
            if (!SameMesh(existing, expected))
            {
                UnityEngine.Object.DestroyImmediate(expected);
                throw new InvalidOperationException("Existing fixed wall mesh does not match the expected dimensions and geometry: " + path);
            }
            UnityEngine.Object.DestroyImmediate(expected);
            return existing;
        }
        if (AssetDatabase.LoadMainAssetAtPath(path) != null || System.IO.File.Exists(path))
        {
            UnityEngine.Object.DestroyImmediate(expected);
            throw new InvalidOperationException("A different asset already occupies the fixed wall mesh path: " + path);
        }
        if (!AssetDatabase.IsValidFolder(AssetFolder)) AssetDatabase.CreateFolder("Assets/C02/Art", "FixedWorldWalls");
        AssetDatabase.CreateAsset(expected, path);
        AssetDatabase.SaveAssetIfDirty(expected);
        return expected;
    }

    private static bool SameMesh(Mesh a, Mesh b)
    {
        if (a.vertexCount != b.vertexCount || a.subMeshCount != 2 || b.subMeshCount != 2 ||
            !Near(a.bounds.center, b.bounds.center, .00001f) || !Near(a.bounds.size, b.bounds.size, .00001f)) return false;
        var av = a.vertices; var bv = b.vertices; var an = a.normals; var bn = b.normals; var auv = a.uv; var buv = b.uv;
        if (an.Length != bn.Length || auv.Length != buv.Length) return false;
        for (int i = 0; i < av.Length; i++)
            if (!av[i].Equals(bv[i]) || !an[i].Equals(bn[i]) || !auv[i].Equals(buv[i])) return false;
        for (int sub = 0; sub < 2; sub++)
        {
            var ai = a.GetIndices(sub); var bi = b.GetIndices(sub);
            if (ai.Length != bi.Length) return false;
            for (int i = 0; i < ai.Length; i++) if (ai[i] != bi[i]) return false;
        }
        return true;
    }

    private static Mesh BuildMesh(Mesh template, bool longX, string name)
    {
        var vertices = new List<Vector3>();
        var uvs = new List<Vector2>();
        var stones = new List<int>();
        var mortar = new List<int>();
        var sourceVertices = template.vertices;
        var sourceUvs = template.uv;
        var sourceTriangles = template.GetTriangles(0);
        var bounds = template.bounds;
        float[] rowLengthsEven = { .6f, 1.2f, 1.2f, 1.2f, 1.2f, 1.2f, 1.2f, 1.2f, .6f };
        float[] rowLengthsOdd = { 1.2f, 1.2f, 1.2f, 1.2f, 1.2f, 1.2f, 1.2f, 1.2f };

        for (int row = 0; row < 5; row++)
        {
            float yCenter = -.5f + (.02f + .25f + row * .54f) / 2.7f;
            float ySize = StoneHeight / 2.7f;
            var lengths = row % 2 == 0 ? rowLengthsEven : rowLengthsOdd;
            float total = 0;
            foreach (float length in lengths) total += length;
            total += (lengths.Length - 1) * Gap;
            float cursor = -total / 20f;
            for (int block = 0; block < lengths.Length; block++)
            {
                float length = lengths[block];
                float axisCenter = cursor + length / 20f;
                float axisSize = length / 10f;
                AddTemplateBlock(sourceVertices, sourceUvs, sourceTriangles, bounds, vertices, uvs, stones,
                    longX, axisCenter, axisSize, yCenter, ySize);
                cursor += (length + Gap) / 10f;
            }
        }

        AddMortar(vertices, uvs, mortar, longX);
        var mesh = new Mesh { name = name };
        mesh.SetVertices(vertices); mesh.SetUVs(0, uvs);
        mesh.subMeshCount = 2;
        mesh.SetTriangles(stones, 0); mesh.SetTriangles(mortar, 1);
        mesh.RecalculateNormals(); mesh.RecalculateBounds();
        if (mesh.bounds.min.x < -.5f || mesh.bounds.min.y < -.5f || mesh.bounds.min.z < -.5f ||
            mesh.bounds.max.x > .5f || mesh.bounds.max.y > .5f || mesh.bounds.max.z > .5f)
        {
            UnityEngine.Object.DestroyImmediate(mesh);
            throw new InvalidOperationException("Generated wall vertices exceed the normalized collision box.");
        }
        return mesh;
    }

    private static void AddTemplateBlock(Vector3[] source, Vector2[] sourceUv, int[] triangles, Bounds bounds,
        List<Vector3> vertices, List<Vector2> uvs, List<int> indices, bool longX,
        float axisCenter, float axisSize, float yCenter, float ySize)
    {
        int offset = vertices.Count;
        Vector3 half = longX ? new Vector3(axisSize * .5f, ySize * .5f, .5f) : new Vector3(.5f, ySize * .5f, axisSize * .5f);
        for (int i = 0; i < source.Length; i++)
        {
            var normalized = new Vector3((source[i].x - bounds.center.x) / bounds.size.x,
                (source[i].y - bounds.center.y) / bounds.size.y, (source[i].z - bounds.center.z) / bounds.size.z);
            vertices.Add(new Vector3(longX ? axisCenter + normalized.x * axisSize : normalized.x * half.x * 2,
                yCenter + normalized.y * ySize, longX ? normalized.z * half.z * 2 : axisCenter + normalized.z * axisSize));
            uvs.Add(sourceUv[i]);
        }
        foreach (int index in triangles) indices.Add(offset + index);
    }

    private static void AddMortar(List<Vector3> vertices, List<Vector2> uvs, List<int> indices, bool longX)
    {
        int start = vertices.Count;
        var half = longX ? new Vector3(.5f - CoreInset / 10, .5f - CoreInset / 2.7f, .5f - CoreInset / 2)
            : new Vector3(.5f - CoreInset / 2, .5f - CoreInset / 2.7f, .5f - CoreInset / 10);
        var corners = new[]
        {
            new Vector3(-half.x,-half.y,-half.z), new Vector3(half.x,-half.y,-half.z), new Vector3(half.x,-half.y,half.z), new Vector3(-half.x,-half.y,half.z),
            new Vector3(-half.x,half.y,-half.z), new Vector3(half.x,half.y,-half.z), new Vector3(half.x,half.y,half.z), new Vector3(-half.x,half.y,half.z)
        };
        int[][] faces = { new[] { 0,4,5,1 }, new[] { 1,5,6,2 }, new[] { 2,6,7,3 }, new[] { 3,7,4,0 }, new[] { 4,7,6,5 }, new[] { 3,0,1,2 } };
        foreach (var face in faces)
        {
            int faceStart = vertices.Count;
            for (int i = 0; i < 4; i++) { vertices.Add(corners[face[i]]); uvs.Add(new Vector2(i == 1 || i == 2 ? 1 : 0, i > 1 ? 1 : 0)); }
            indices.AddRange(new[] { faceStart, faceStart + 1, faceStart + 2, faceStart, faceStart + 2, faceStart + 3 });
        }
        if (vertices.Count - start != 24) throw new InvalidOperationException("Mortar mesh construction failed.");
    }

    private static bool Near(Vector3 a, Vector3 b, float epsilon) => (a - b).sqrMagnitude <= epsilon * epsilon;
}
