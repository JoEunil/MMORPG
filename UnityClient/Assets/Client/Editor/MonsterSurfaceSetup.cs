using System;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Mmorpg.Client.Editor
{
    public static class MonsterSurfaceSetup
    {
        [MenuItem("Tools/Client/Art/Apply Monster Leather and Iron Surfaces")]
        public static void Apply()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode || EditorApplication.isCompiling || EditorApplication.isUpdating)
                throw new InvalidOperationException("Apply monster surfaces only in idle Edit Mode.");
            for (int i = 0; i < SceneManager.sceneCount; i++)
                if (SceneManager.GetSceneAt(i).isDirty)
                    throw new InvalidOperationException("Save or discard scene edits before applying monster surfaces.");

            const string leatherPath = "Assets/Client/Art/Actors/ScavengedLeather.mat";
            const string ironPath = "Assets/Client/Art/Actors/DarkForgedIron.mat";
            var leather = AssetDatabase.LoadAssetAtPath<Material>(leatherPath);
            var iron = AssetDatabase.LoadAssetAtPath<Material>(ironPath);
            var leatherTexture = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Client/Art/Textures/oiled-leather-albedo-v1.png");
            var ironTexture = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Client/Art/Textures/worn-steel-albedo-v1.png");
            if (leather == null || iron == null || leatherTexture == null || ironTexture == null)
                throw new InvalidOperationException("Existing monster materials and imported leather/steel textures are required.");
            if (!leather.HasProperty("_BaseMap") || !iron.HasProperty("_BaseMap"))
                throw new InvalidOperationException("Both monster materials must support a base-color texture.");
            if (EditorUtility.IsDirty(leather) || EditorUtility.IsDirty(iron))
                throw new InvalidOperationException("Save or discard monster material edits before applying surfaces.");
            if (!AssetDatabase.IsOpenForEdit(leatherPath) || !AssetDatabase.IsOpenForEdit(ironPath))
                throw new InvalidOperationException("Both monster materials must be editable.");

            Undo.RecordObjects(new UnityEngine.Object[] { leather, iron }, "Apply Monster Leather and Iron Surfaces");
            leather.SetTexture("_BaseMap", leatherTexture);
            leather.SetTextureScale("_BaseMap", new Vector2(2, 2));
            leather.SetColor("_BaseColor", new Color(.72f, .66f, .59f, 1));
            iron.SetTexture("_BaseMap", ironTexture);
            iron.SetTextureScale("_BaseMap", new Vector2(2, 2));
            iron.SetColor("_BaseColor", new Color(.65f, .68f, .67f, 1));
            EditorUtility.SetDirty(leather);
            EditorUtility.SetDirty(iron);
            AssetDatabase.SaveAssetIfDirty(leather);
            AssetDatabase.SaveAssetIfDirty(iron);
            Debug.Log("Saved monster leather and iron textures with compensated base colors. Shading values, meshes, prefabs, and scenes were preserved.");
        }
    }
}
