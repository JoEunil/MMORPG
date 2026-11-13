using System;
using System.IO;
using Mmorpg.Client;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using Object = UnityEngine.Object;

namespace Mmorpg.Client.Editor
{
    // Edit-time schematic. The artwork uses authored road geometry and the baked walkability grid.
    public static class WorldMinimapAtlasExport
    {
        public const string AtlasPath = "Assets/Client/Art/CitadelEnvironment/WorldMinimapAtlas.png";
        private const int Width = 2048;
        private const string RoadMaterialPath = "Assets/Client/Art/CitadelEnvironment/WornRoadStone.mat";
        private const float Margin = 20f;

        [Serializable]
        private sealed class AtlasMetadata
        {
            public int version;
            public float minX, minZ, width, depth;
        }

        [MenuItem("Tools/Client/Art/Bake Minimap Atlas")]
        private static void BakeFromMenu()
        {
            Bake(out var bounds);
            Debug.Log("Schematic map baked: " + bounds + ". Re-bake after changing roads or collision.");
        }

        // Explicit authoring only. HUD setup must use TryLoad rather than silently regenerating it.
        public static Texture2D Bake(out Rect bounds)
        {
            var scene = SceneManager.GetActiveScene();
            if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/Client/Scenes/World.unity" || scene.isDirty)
                throw new InvalidOperationException("Open the clean, saved C02 scene in Edit Mode before baking its atlas.");
            var authors = Object.FindObjectsByType<MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
            if (authors.Length != 1 || authors[0].ground == null || authors[0].map == null || !authors[0].map.IsValid)
                throw new InvalidOperationException("A world ground and a valid baked navigation map are required.");

            var author = authors[0];
            var ground = MapAuthoring.WorldBounds(author.ground);
            bounds = new Rect(ground.min.x - Margin, ground.min.z - Margin,
                ground.size.x + Margin * 2, ground.size.z + Margin * 2);
            int height = Mathf.RoundToInt(Width * bounds.height / bounds.width);
            byte[] png = DrawSchematic(author, bounds, height);
            File.WriteAllBytes(Path.Combine(Application.dataPath, AtlasPath.Substring("Assets/".Length)), png);
            AssetDatabase.ImportAsset(AtlasPath, ImportAssetOptions.ForceSynchronousImport);
            var importer = (TextureImporter)AssetImporter.GetAtPath(AtlasPath);
            importer.textureType = TextureImporterType.Default;
            importer.sRGBTexture = true;
            importer.wrapMode = TextureWrapMode.Clamp;
            importer.mipmapEnabled = true;
            importer.filterMode = FilterMode.Bilinear;
            importer.anisoLevel = 2;
            importer.npotScale = TextureImporterNPOTScale.None;
            importer.textureCompression = TextureImporterCompression.CompressedHQ;
            importer.maxTextureSize = Width;
            importer.isReadable = false;
            importer.userData = JsonUtility.ToJson(new AtlasMetadata {
                version = 1, minX = bounds.xMin, minZ = bounds.yMin, width = bounds.width, depth = bounds.height });
            importer.SaveAndReimport();
            return AssetDatabase.LoadAssetAtPath<Texture2D>(AtlasPath);
        }

        // Bounds belong to the baked PNG. Never mix an old atlas with newly inferred world bounds.
        public static bool TryLoad(out Texture2D atlas, out Rect bounds)
        {
            atlas = null;
            bounds = default;
            var importer = AssetImporter.GetAtPath(AtlasPath) as TextureImporter;
            if (importer == null || string.IsNullOrWhiteSpace(importer.userData)) return false;
            AtlasMetadata metadata;
            try { metadata = JsonUtility.FromJson<AtlasMetadata>(importer.userData); }
            catch (ArgumentException) { return false; }
            if (metadata == null || metadata.version != 1 || !Finite(metadata.minX) || !Finite(metadata.minZ) ||
                !Finite(metadata.width) || !Finite(metadata.depth) || metadata.width <= 0 || metadata.depth <= 0) return false;
            atlas = AssetDatabase.LoadAssetAtPath<Texture2D>(AtlasPath);
            if (atlas == null) return false;
            bounds = new Rect(metadata.minX, metadata.minZ, metadata.width, metadata.depth);
            return true;
        }

        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private static byte[] DrawSchematic(MapAuthoring author, Rect bounds, int height)
        {
            var outside = new Color32(38, 46, 44, 255);
            var open = new Color32(54, 66, 58, 255);
            var solid = new Color32(76, 84, 74, 255);
            var road = new Color32(145, 131, 99, 255);
            var map = author.map;
            var pixels = new Color32[Width * height];
            for (int y = 0; y < height; y++)
            {
                float z = bounds.yMin + (y + .5f) * bounds.height / height;
                int row = Mathf.FloorToInt((z - map.origin.y) / map.tileSize);
                for (int x = 0; x < Width; x++)
                {
                    float worldX = bounds.xMin + (x + .5f) * bounds.width / Width;
                    int column = Mathf.FloorToInt((worldX - map.origin.x) / map.tileSize);
                    bool inside = column >= 0 && row >= 0 && column < map.width && row < map.height;
                    pixels[y * Width + x] = !inside ? outside :
                        map.walkable[row * map.width + column] == 1 ? open : solid;
                }
            }

            var roadMaterial = AssetDatabase.LoadAssetAtPath<Material>(RoadMaterialPath);
            if (roadMaterial == null || author.transform.parent == null)
                throw new InvalidOperationException("The world root and its road material are required for the schematic.");

            int roads = 0;
            foreach (var filter in author.transform.parent.GetComponentsInChildren<MeshFilter>(true))
            {
                if (!filter.TryGetComponent<MeshRenderer>(out var renderer) || !renderer.enabled ||
                    renderer.sharedMaterial != roadMaterial || filter.sharedMesh == null) continue;
                // The world root is hidden in Edit Mode, but individually hidden objects are still excluded.
                if (IsHidden(filter.transform, author.transform.parent)) continue;

                var mesh = filter.sharedMesh;
                var vertices = mesh.vertices;
                var projected = new Vector2[vertices.Length];
                for (int i = 0; i < vertices.Length; i++)
                {
                    var point = filter.transform.TransformPoint(vertices[i]);
                    projected[i] = new Vector2((point.x - bounds.xMin) * Width / bounds.width,
                        (point.z - bounds.yMin) * height / bounds.height);
                }
                var triangles = mesh.GetTriangles(0);
                for (int i = 0; i < triangles.Length; i += 3)
                    FillRoad(pixels, height, projected[triangles[i]], projected[triangles[i + 1]], projected[triangles[i + 2]], road, open);
                roads++;
            }
            if (roads == 0) throw new InvalidOperationException("No authored roads were found for the schematic.");

            var texture = new Texture2D(Width, height, TextureFormat.RGB24, false);
            try
            {
                texture.SetPixels32(SoftenTerrain(pixels, height));
                texture.Apply(false, false);
                return texture.EncodeToPNG();
            }
            finally { Object.DestroyImmediate(texture); }
        }

        private static Color32[] SoftenTerrain(Color32[] source, int height)
        {
            // Feather the drawn silhouettes without moving the underlying road or collision coordinates.
            const int feather = 4;
            var result = new Color32[source.Length];
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < Width; x++)
                {
                    int red = 0, green = 0, blue = 0;
                    for (int dy = -1; dy <= 1; dy++)
                    {
                        int row = Mathf.Clamp(y + dy * feather, 0, height - 1) * Width;
                        for (int dx = -1; dx <= 1; dx++)
                        {
                            var sample = source[row + Mathf.Clamp(x + dx * feather, 0, Width - 1)];
                            int weight = (dx == 0 ? 2 : 1) * (dy == 0 ? 2 : 1);
                            red += sample.r * weight;
                            green += sample.g * weight;
                            blue += sample.b * weight;
                        }
                    }

                    // A quiet ground variation keeps broad fields from reading as flat blocks.
                    int shade = Mathf.RoundToInt((Mathf.PerlinNoise(x * .012f, y * .012f) - .5f) * 14);
                    result[y * Width + x] = new Color32((byte)(red / 16 + shade),
                        (byte)(green / 16 + shade), (byte)(blue / 16 + shade), 255);
                }
            }
            return result;
        }

        private static bool IsHidden(Transform part, Transform root)
        {
            for (; part != root; part = part.parent)
                if (!part.gameObject.activeSelf) return true;
            return false;
        }

        private static void FillRoad(Color32[] pixels, int height, Vector2 a, Vector2 b, Vector2 c, Color32 road, Color32 open)
        {
            float winding = Edge(a, b, c);
            if (Mathf.Abs(winding) < .0001f) return;
            float sign = Mathf.Sign(winding);
            int minX = Mathf.Max(0, Mathf.FloorToInt(Mathf.Min(a.x, Mathf.Min(b.x, c.x))));
            int maxX = Mathf.Min(Width - 1, Mathf.CeilToInt(Mathf.Max(a.x, Mathf.Max(b.x, c.x))));
            int minY = Mathf.Max(0, Mathf.FloorToInt(Mathf.Min(a.y, Mathf.Min(b.y, c.y))));
            int maxY = Mathf.Min(height - 1, Mathf.CeilToInt(Mathf.Max(a.y, Mathf.Max(b.y, c.y))));
            for (int y = minY; y <= maxY; y++)
                for (int x = minX; x <= maxX; x++)
                {
                    int index = y * Width + x;
                    if (!pixels[index].Equals(open)) continue; // Clip road art to the walkable ground.
                    var p = new Vector2(x + .5f, y + .5f);
                    if (Edge(a, b, p) * sign >= 0 && Edge(b, c, p) * sign >= 0 && Edge(c, a, p) * sign >= 0)
                        pixels[index] = road;
                }
        }

        private static float Edge(Vector2 a, Vector2 b, Vector2 p) =>
            (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    }
}
