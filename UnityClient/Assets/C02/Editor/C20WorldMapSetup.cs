using System;
using System.Collections.Generic;
using C02;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class C20WorldMapSetup
{
    public const string MapPath = "Assets/C02/Content/C20WorldMap.asset";

    [MenuItem("C02/World/Set Up Fixed Collision Map")]
    public static void Apply()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlaying || scene.path != "Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the saved C02 scene in Edit Mode.");
        var screen = UnityEngine.Object.FindFirstObjectByType<C02WorldScreen>(FindObjectsInactive.Include);
        if (screen == null) throw new InvalidOperationException("The existing world screen is required.");
        var serialized = new SerializedObject(screen);
        var mapReference = serialized.FindProperty("worldMap");
        if (mapReference == null) throw new InvalidOperationException("Compile the world-map client integration first.");
        var map = AssetDatabase.LoadAssetAtPath<C20WorldMap>(MapPath);
        if (map == null)
        {
            map = ScriptableObject.CreateInstance<C20WorldMap>();
            AssetDatabase.CreateAsset(map, MapPath);
        }
        var authors = UnityEngine.Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        if (authors.Length > 1) throw new InvalidOperationException("Multiple world authoring roots are not supported.");
        C20MapAuthoring author;
        if (authors.Length == 1) author = authors[0];
        else
        {
            var worldProperty = serialized.FindProperty("worldRoot");
            var worldRoot = worldProperty.objectReferenceValue as GameObject;
            if (worldRoot == null) throw new InvalidOperationException("The world root is missing.");
            var fixedRoot = new GameObject("C20FixedWorldMap");
            fixedRoot.transform.SetParent(worldRoot.transform, false);
            fixedRoot.transform.position = Vector3.zero;
            author = fixedRoot.AddComponent<C20MapAuthoring>();
            author.map = map;
            var groundObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
            groundObject.name = "NavigationGround";
            groundObject.transform.SetParent(fixedRoot.transform, false);
            groundObject.transform.position = new Vector3(C20WorldMap.ZoneSize * 1.5f, -.1f, C20WorldMap.ZoneSize);
            groundObject.transform.localScale = new Vector3(C20WorldMap.ZoneSize * 3 + 10, .1f, C20WorldMap.ZoneSize * 2 + 10);
            author.ground = groundObject.GetComponent<BoxCollider>();
            author.ground.isTrigger = true;
            var groundMaterial = AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/Meadow.mat");
            if (groundMaterial == null) throw new InvalidOperationException("Existing ground material is required.");
            groundObject.GetComponent<MeshRenderer>().sharedMaterial = groundMaterial;
            var material = AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/PaleStone.mat");
            if (material == null) throw new InvalidOperationException("Existing stone material is required.");
            var obstacles = new List<BoxCollider>();
            // Fixed world objects. Grass and the cosmetic moving backdrop are deliberately not exported.
            for (int zone = 0; zone < 6; zone++)
            {
                Vector3 offset = new Vector3((zone % 3) * C20WorldMap.ZoneSize, 0, (zone / 3) * C20WorldMap.ZoneSize);
                for (int i = 0; i < 3; i++)
                {
                    var wall = GameObject.CreatePrimitive(PrimitiveType.Cube);
                    wall.name = "Zone" + (zone + 1) + "_RuinedWall" + (i + 1);
                    wall.transform.SetParent(fixedRoot.transform, false);
                    wall.transform.position = offset + new Vector3((i == 0 ? 32 : i == 1 ? 62 : 76) * C20WorldMap.ZoneSize / 100, 1.35f, (i == 0 ? 22 : i == 1 ? 45 : 70) * C20WorldMap.ZoneSize / 100);
                    wall.transform.localScale = i == 1 ? new Vector3(10, 2.7f, 2) : new Vector3(2, 2.7f, 10);
                    wall.GetComponent<MeshRenderer>().sharedMaterial = material;
                    obstacles.Add(wall.GetComponent<BoxCollider>());
                }
            }
            author.obstacles = obstacles.ToArray();
        }
        if (author.map != map) throw new InvalidOperationException("The existing authoring root uses a different map asset.");
        C20WorldMapExport.Bake(author);
        mapReference.objectReferenceValue = map;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        AssetDatabase.SaveAssets();
        EditorSceneManager.MarkSceneDirty(scene);
        if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Map scene save failed.");
        Debug.Log("Fixed collision map authored and client map reference saved. Export explicitly after reviewing the map.");
    }
}
