using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class KnightMaterialPreview
{
    public static string Main()
    {
        if (EditorApplication.isPlayingOrWillChangePlaymode) throw new InvalidOperationException("Stop Play before preview.");
        var scene = UnityEngine.SceneManagement.SceneManager.GetActiveScene();
        if (scene.isDirty) throw new InvalidOperationException("Preserve unsaved scene before preview.");
        string output = Path.Combine(Application.dataPath, "../Builds/Client-Resume/ArtOptions");
        Directory.CreateDirectory(output);
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/C02/Art/C04/ObservedPlayer.prefab");
        if (prefab == null) throw new InvalidOperationException("Existing knight prefab missing.");
        Color[] colors = { new Color(.78f,.82f,.86f), new Color(.62f,.66f,.70f), new Color(.43f,.47f,.51f) };
        float[] gloss = { .62f, .40f, .28f };
        string[] names = { "A-bright-steel", "B-restrained-steel", "C-matte-dark-steel" };
        for (int variant = 0; variant < names.Length; variant++)
        {
            var preview = new PreviewRenderUtility();
            var clones = new List<Material>();
            Texture2D result = null;
            try
            {
                var knight = UnityEngine.Object.Instantiate(prefab);
                preview.AddSingleGO(knight);
                knight.transform.position = Vector3.zero;
                knight.transform.rotation = Quaternion.Euler(0, -25, 0);
                knight.SetActive(true);
                var renderers = knight.GetComponentsInChildren<MeshRenderer>(true);
                Bounds bounds = default;
                bool first = true;
                int slots = 0;
                foreach (var renderer in renderers)
                {
                    if (first) { bounds = renderer.bounds; first = false; } else bounds.Encapsulate(renderer.bounds);
                    var materials = renderer.sharedMaterials;
                    for (int i = 0; i < materials.Length; i++)
                    {
                        var source = materials[i];
                        if (source == null || source.name != "TemperedSteel") continue;
                        var copy = new Material(source);
                        copy.SetColor("_BaseColor", colors[variant]);
                        if (copy.HasProperty("_Color")) copy.SetColor("_Color", colors[variant]);
                        copy.SetFloat("_Metallic", .76f);
                        copy.SetFloat("_Smoothness", gloss[variant]);
                        clones.Add(copy); materials[i] = copy; slots++;
                    }
                    renderer.sharedMaterials = materials;
                }
                if (first || slots == 0) throw new InvalidOperationException("Expected steel slots not found.");
                float height = bounds.size.y;
                var probeObject = new GameObject("Same citadel reflection for all variants");
                preview.AddSingleGO(probeObject);
                var probe = probeObject.AddComponent<ReflectionProbe>();
                probe.mode = UnityEngine.Rendering.ReflectionProbeMode.Custom;
                probe.customBakedTexture = AssetDatabase.LoadAssetAtPath<Cubemap>("Assets/C02/Art/CitadelEnvironment/CitadelReflection.asset");
                probe.size = Vector3.one * height * 20;
                probe.intensity = 1.1f;
                var floor = GameObject.CreatePrimitive(PrimitiveType.Plane);
                preview.AddSingleGO(floor);
                floor.transform.position = new Vector3(bounds.center.x, bounds.min.y - .015f, bounds.center.z);
                floor.transform.localScale = Vector3.one * (height * .35f);
                var ground = new Material(Shader.Find("Universal Render Pipeline/Lit"));
                ground.SetColor("_BaseColor",new Color(.16f,.18f,.15f)); ground.SetFloat("_Smoothness",.12f);
                clones.Add(ground); floor.GetComponent<Renderer>().sharedMaterial = ground;
                preview.cameraFieldOfView = 30;
                preview.camera.clearFlags = CameraClearFlags.Color;
                preview.camera.backgroundColor = new Color(.115f,.13f,.145f);
                Vector3 focus = bounds.center;
                preview.camera.transform.position = focus + new Vector3(1.15f, .85f, -1.65f).normalized * height * 2.65f;
                preview.camera.transform.LookAt(focus);
                preview.camera.nearClipPlane = .01f;
                preview.camera.farClipPlane = height * 20;
                preview.lights[0].intensity = 1.8f;
                preview.lights[0].color = new Color(1f,.95f,.87f);
                preview.lights[0].transform.rotation = Quaternion.Euler(38, -35, 0);
                preview.lights[1].intensity = 1.2f;
                preview.lights[1].color = new Color(.80f,.88f,1f);
                preview.lights[1].transform.rotation = Quaternion.Euler(30, 140, 0);
                preview.ambientColor = new Color(.38f,.40f,.44f);
                preview.BeginStaticPreview(new Rect(0,0,900,900));
                preview.Render(true);
                result = preview.EndStaticPreview();
                File.WriteAllBytes(Path.Combine(output,names[variant]+".png"),result.EncodeToPNG());
            }
            finally
            {
                if (result != null) UnityEngine.Object.DestroyImmediate(result);
                preview.Cleanup();
                foreach (var material in clones) if (material != null) UnityEngine.Object.DestroyImmediate(material);
            }
        }
        if (scene.isDirty) throw new InvalidOperationException("Preview unexpectedly dirtied source scene.");
        return "Three Editor material previews saved; source materials/prefab/scene unchanged, no Play/input/server session.";
    }
}
