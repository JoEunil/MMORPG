using System;
using System.Collections.Generic;
using System.IO;
using C02;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;
using Object = UnityEngine.Object;

// Edit-time static scenery capture. Runtime markers and the map's navigation data stay separate.
public static class WorldMinimapAtlasExport
{
    public const string AtlasPath = "Assets/C02/Art/CitadelEnvironment/WorldMinimapAtlas.png";
    private const int Width = 2048;
    private const float Margin = 20f;

    [Serializable]
    private sealed class AtlasMetadata
    {
        public int version;
        public float minX, minZ, width, depth;
    }

    [MenuItem("Tools/Client Art/Bake Minimap Atlas")]
    private static void BakeFromMenu()
    {
        Bake(out var bounds);
        Debug.Log("Static minimap atlas baked: " + bounds + ". Re-bake after changing scenery; no navigation or server data was exported.");
    }

    // Explicit authoring only. HUD setup must use TryLoad rather than silently capturing again.
    public static Texture2D Bake(out Rect bounds)
    {
        var scene = SceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the clean, saved C02 scene in Edit Mode before baking its atlas.");
        var authors = Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        var screen = Object.FindFirstObjectByType<C02WorldScreen>(FindObjectsInactive.Include);
        if (authors.Length != 1 || authors[0].ground == null || screen == null)
            throw new InvalidOperationException("The existing world screen and one authored ground are required.");
        var refs = new SerializedObject(screen);
        var sourceRoot = (GameObject)refs.FindProperty("worldRoot").objectReferenceValue;
        var own = (Transform)refs.FindProperty("ownCharacter").objectReferenceValue;
        if (sourceRoot == null || own == null || !own.IsChildOf(sourceRoot.transform) ||
            !authors[0].ground.transform.IsChildOf(sourceRoot.transform))
            throw new InvalidOperationException("Ground and own character must belong to the bound world root.");
        var ground = C20MapAuthoring.WorldBounds(authors[0].ground);
        bounds = new Rect(ground.min.x - Margin, ground.min.z - Margin,
            ground.size.x + Margin * 2, ground.size.z + Margin * 2);
        int height = Mathf.RoundToInt(Width * bounds.height / bounds.width);
        var shader = Shader.Find("Universal Render Pipeline/Unlit");
        if (shader == null) throw new InvalidOperationException("The project's URP Unlit shader is required for atlas capture.");
        var previousTarget = RenderTexture.active;
        bool previousFog = RenderSettings.fog;
        int sourceRenderers = sourceRoot.GetComponentsInChildren<Renderer>(true).Length;
        int sourceColliders = sourceRoot.GetComponentsInChildren<Collider>(true).Length;
        var preview = EditorSceneManager.NewPreviewScene();
        var target = new RenderTexture(Width, height, 24);
        var materials = new Dictionary<Material, Material>();
        Camera camera = null;
        byte[] png;
        try
        {
            var clone = Object.Instantiate(sourceRoot);
            SceneManager.MoveGameObjectToScene(clone, preview);
            foreach (var script in clone.GetComponentsInChildren<MonoBehaviour>(true)) script.enabled = false;
            HideClone(own, sourceRoot.transform, clone.transform);
            // This legacy local floor follows the player at runtime, unlike the authored world ground.
            var follower = (Transform)refs.FindProperty("fieldPlane").objectReferenceValue;
            if (follower != null && follower != authors[0].ground.transform && follower.IsChildOf(sourceRoot.transform))
                HideClone(follower, sourceRoot.transform, clone.transform);
            var observed = screen.GetComponent<C04ObservedWorld>();
            if (observed != null)
            {
                var display = (Transform)new SerializedObject(observed).FindProperty("displayRoot").objectReferenceValue;
                if (display != null && display.IsChildOf(sourceRoot.transform)) HideClone(display, sourceRoot.transform, clone.transform);
            }
            foreach (var motion in clone.GetComponentsInChildren<C19ActorMotion>(true)) motion.gameObject.SetActive(false);
            foreach (var vitals in clone.GetComponentsInChildren<C13EntityVitals>(true)) vitals.gameObject.SetActive(false);
            foreach (var canvas in clone.GetComponentsInChildren<Canvas>(true)) canvas.gameObject.SetActive(false);
            foreach (var part in clone.GetComponentsInChildren<Transform>(true))
                if (part.name == "C04DisplayRoot") part.gameObject.SetActive(false);
            foreach (var existing in clone.GetComponentsInChildren<Camera>(true)) existing.enabled = false;
            foreach (var light in clone.GetComponentsInChildren<Light>(true)) light.enabled = false;
            clone.SetActive(true);
            // Only actual mesh scenery is baked; transient trails, particles and skinned actors are excluded.
            float top = ground.max.y;
            foreach (var renderer in clone.GetComponentsInChildren<Renderer>(true))
            {
                if (!(renderer is MeshRenderer)) { renderer.enabled = false; continue; }
                if (!renderer.enabled || !renderer.gameObject.activeInHierarchy) continue;
                top = Mathf.Max(top, renderer.bounds.max.y);
                var slots = renderer.sharedMaterials;
                for (int i = 0; i < slots.Length; i++)
                {
                    var source = slots[i];
                    if (source == null) continue;
                    if (!materials.TryGetValue(source, out var neutral))
                    {
                        neutral = NeutralMaterial(source, shader);
                        materials.Add(source, neutral);
                    }
                    slots[i] = neutral;
                }
                renderer.sharedMaterials = slots;
                renderer.shadowCastingMode = ShadowCastingMode.Off;
                renderer.receiveShadows = false;
            }
            var cameraObject = new GameObject("Static minimap atlas camera", typeof(Camera));
            SceneManager.MoveGameObjectToScene(cameraObject, preview);
            camera = cameraObject.GetComponent<Camera>();
            camera.enabled = false;
            camera.scene = preview;
            camera.orthographic = true;
            camera.orthographicSize = bounds.height * .5f;
            camera.aspect = bounds.width / bounds.height;
            camera.nearClipPlane = .1f;
            camera.farClipPlane = Mathf.Max(100, top - ground.min.y + 30);
            camera.transform.position = new Vector3(bounds.center.x, top + 20, bounds.center.y);
            // Camera up is world +Z; texture U follows +X, V follows +Z, with no post-capture flip.
            camera.transform.rotation = Quaternion.Euler(90, 0, 0);
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(.075f, .085f, .075f);
            camera.targetTexture = target;
            camera.allowHDR = false;
            // Rendering is synchronous: the original scene does not render while fog is temporarily disabled.
            RenderSettings.fog = false;
            camera.Render();
            RenderTexture.active = target;
            var pixels = new Texture2D(Width, height, TextureFormat.RGB24, false);
            try
            {
                pixels.ReadPixels(new Rect(0, 0, Width, height), 0, 0);
                pixels.Apply(false, false);
                png = pixels.EncodeToPNG();
            }
            finally { Object.DestroyImmediate(pixels); }
        }
        finally
        {
            if (camera != null) camera.targetTexture = null;
            RenderTexture.active = previousTarget;
            RenderSettings.fog = previousFog;
            Object.DestroyImmediate(target);
            foreach (var material in materials.Values) Object.DestroyImmediate(material);
            EditorSceneManager.ClosePreviewScene(preview);
        }
        if (SceneManager.GetActiveScene() != scene || scene.isDirty || RenderSettings.fog != previousFog ||
            sourceRoot.GetComponentsInChildren<Renderer>(true).Length != sourceRenderers ||
            sourceRoot.GetComponentsInChildren<Collider>(true).Length != sourceColliders)
            throw new InvalidOperationException("Atlas capture did not preserve the source scene state; no atlas was written.");
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

    private static void HideClone(Transform source, Transform sourceRoot, Transform cloneRoot)
    {
        var copy = cloneRoot.Find(AnimationUtility.CalculateTransformPath(source, sourceRoot));
        if (copy == null) throw new InvalidOperationException("A runtime actor root could not be excluded from the atlas clone.");
        copy.gameObject.SetActive(false);
    }

    private static Material NeutralMaterial(Material source, Shader shader)
    {
        var material = new Material(shader) { hideFlags = HideFlags.HideAndDontSave };
        string textureProperty = source.HasProperty("_BaseMap") ? "_BaseMap" : source.HasProperty("_MainTex") ? "_MainTex" : null;
        if (textureProperty != null)
        {
            material.SetTexture("_BaseMap", source.GetTexture(textureProperty));
            material.SetTextureScale("_BaseMap", source.GetTextureScale(textureProperty));
            material.SetTextureOffset("_BaseMap", source.GetTextureOffset(textureProperty));
        }
        material.SetColor("_BaseColor", source.HasProperty("_BaseColor") ? source.GetColor("_BaseColor") :
            source.HasProperty("_Color") ? source.GetColor("_Color") : Color.white);
        material.SetFloat("_Cull", (float)CullMode.Off);
        if (source.HasProperty("_AlphaClip") && source.GetFloat("_AlphaClip") > .5f)
        {
            material.SetFloat("_AlphaClip", 1);
            material.SetFloat("_Cutoff", source.GetFloat("_Cutoff"));
            material.EnableKeyword("_ALPHATEST_ON");
            material.renderQueue = (int)RenderQueue.AlphaTest;
        }
        if (source.HasProperty("_Surface") && source.GetFloat("_Surface") > .5f)
        {
            material.SetFloat("_Surface", 1);
            material.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha);
            material.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
            material.SetFloat("_ZWrite", 0);
            material.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
            material.renderQueue = (int)RenderQueue.Transparent;
        }
        return material;
    }
}
