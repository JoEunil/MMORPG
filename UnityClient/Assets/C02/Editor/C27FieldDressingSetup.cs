using System;
using C02;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using Object = UnityEngine.Object;

// Fixed, editor-authored presentation only. Never add these objects to C20 obstacles.
// All meshes/materials come from the repository's existing authored outpost.
public static class C27FieldDressingSetup
{
    private const string RootName = "C27FixedFieldDressing";

    [MenuItem("C02/Art/Apply Fixed Field Dressing")]
    public static void Apply()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the saved C02 scene in Edit Mode.");
        var authors = Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        if (authors.Length != 1 || authors[0].obstacles == null || authors[0].obstacles.Length != 18)
            throw new InvalidOperationException("The existing fixed six-zone map with 18 obstacles is required.");
        var author = authors[0];
        Transform world = author.transform.parent;
        if (world == null || world.name != "C02World" || author.zoneColumns != 3 || author.zoneRows != 2 ||
            author.zoneCoreSize != 200 || world.position != Vector3.zero || world.rotation != Quaternion.identity || world.lossyScale != Vector3.one)
            throw new InvalidOperationException("Expected the existing unscaled 600 by 400 world.");
        if (world.Find(RootName) != null)
            throw new InvalidOperationException("Field dressing already exists. Review or undo the owned dressing before replacing it.");
        Transform outpost = world.Find("V01Outpost");
        Transform pine = outpost == null ? null : outpost.Find("Pine0");
        Transform grass = outpost == null ? null : outpost.Find("GrassClump0");
        Transform rock = outpost == null ? null : outpost.Find("RoadsideRock1");
        var path = AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/V04/EntryTrailFeather.mat");
        var trailMesh = AssetDatabase.LoadAssetAtPath<Mesh>("Assets/C02/Art/V04/EntryTrailRibbon.asset");
        var foliage = AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/V04/MeadowTufts.mat");
        var stone = AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/V04/MossWeatheredStone.mat");
        if (pine == null || grass == null || rock == null || path == null || trailMesh == null || trailMesh.vertexCount != 145 || foliage == null || stone == null)
            throw new InvalidOperationException("Existing pine, grass, rock and field materials are required; no assets are replaced.");
        foreach (var obstacle in author.obstacles)
            if (obstacle == null) throw new InvalidOperationException("An authored obstacle is missing.");

        Undo.IncrementCurrentGroup();
        int undoGroup = Undo.GetCurrentGroup();
        Undo.SetCurrentGroupName("Author fixed field dressing");
        var root = new GameObject(RootName);
        Undo.RegisterCreatedObjectUndo(root, "Author fixed field dressing");
        root.transform.SetParent(world, false);
        try
        {
            // Tall silhouettes occupy only already-solid wall footprints. Canopies can
            // overhang, but a tree trunk never invents an unexported collision obstacle.
            for (int i = 0; i < author.obstacles.Length; i++)
            {
                Bounds bounds = C20MapAuthoring.WorldBounds(author.obstacles[i]);
                bool alongX = bounds.size.x > bounds.size.z;
                for (int treeIndex = 0; treeIndex < 2; treeIndex++)
                {
                    float along = treeIndex == 0 ? -.27f : .27f;
                    Vector3 position = new Vector3(bounds.center.x, 0, bounds.center.z) +
                        (alongX ? Vector3.right * bounds.size.x : Vector3.forward * bounds.size.z) * along;
                    var tree = Copy(pine, root.transform, "WallGrove_" + i + "_" + treeIndex, position,
                        new Vector3(.72f, .83f + (i % 3) * .09f, .72f), i * 31 + treeIndex * 71);
                    // Keep the wall's outline readable through the asymmetric crowns.
                    tree.transform.localPosition += Vector3.up * .03f;
                }
            }

            for (int zone = 0; zone < 6; zone++)
            {
                Vector3 origin = new Vector3((zone % 3) * 200, 0, (zone / 3) * 200);
                var region = new GameObject(zone == 0 ? "TownApproach" : "HuntingMeadow" + zone).transform;
                region.SetParent(root.transform, false);
                region.localPosition = origin;
                // Reuse the existing feathered curve: soft worn edges rather than slabs.
                // Broad bends pass around walls; a segment crossing one is omitted.
                Vector2[] route = { new Vector2(0, 10), new Vector2(28, 10), new Vector2(48, 24),
                    new Vector2(84, 26), new Vector2(108, 55), new Vector2(116, 104),
                    new Vector2(143, 115), new Vector2(165, 145), new Vector2(200, 160) };
                for (int section = 1; section < route.Length; section++)
                {
                    Vector2 start = route[section - 1], end = route[section];
                    AddTrail(region, author, trailMesh, path, start, end, section);
                }

                // Deterministic authored clusters: sparse paths, ankle-height vegetation,
                // and buried stone. No runtime spawning or camera-following scenery.
                var random = new System.Random(27120 + zone * 101);
                for (int cluster = 0; cluster < 48; cluster++)
                {
                    Vector2 point = new Vector2(8 + (float)random.NextDouble() * 184,
                        8 + (float)random.NextDouble() * 184);
                    if (zone == 0 && point.x < 24 && point.y < 26) continue;
                    bool besideRoad = false;
                    for (int section = 1; section < route.Length; section++)
                    {
                        Vector2 edge = route[section] - route[section - 1];
                        float t = Mathf.Clamp01(Vector2.Dot(point - route[section - 1], edge) / edge.sqrMagnitude);
                        if (Vector2.Distance(point, route[section - 1] + edge * t) < 4.5f) besideRoad = true;
                    }
                    if (besideRoad) continue;
                    Vector3 centre = new Vector3(point.x, 0, point.y);
                    float angle = (float)random.NextDouble() * 360;
                    for (int tuft = 0; tuft < 3; tuft++)
                    {
                        Vector3 offset = new Vector3(Mathf.Sin(tuft * 2.4f + cluster) * 1.5f, -.01f,
                            Mathf.Cos(tuft * 2.4f + cluster) * 1.3f);
                        var plant = Copy(grass, region, "LowHeather_" + cluster + "_" + tuft,
                            centre + offset, new Vector3(3.4f, 1.35f, 3.4f), angle + tuft * 53);
                        plant.GetComponent<MeshRenderer>().sharedMaterial = foliage;
                    }
                    if (cluster % 3 != 0) continue;
                    var buried = Copy(rock, region, "BuriedFieldStone_" + cluster, centre + Vector3.up * -.08f,
                        new Vector3(1.5f, .18f, 1.05f), angle);
                    buried.GetComponent<MeshRenderer>().sharedMaterial = stone;
                }
            }
            if (root.GetComponentsInChildren<Collider>(true).Length != 0)
                throw new InvalidOperationException("Decorative objects must not introduce colliders.");
            foreach (var renderer in root.GetComponentsInChildren<MeshRenderer>(true))
                GameObjectUtility.SetStaticEditorFlags(renderer.gameObject, StaticEditorFlags.BatchingStatic);
            Undo.FlushUndoRecordObjects();
            EditorSceneManager.MarkSceneDirty(scene);
            if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Saving the decorated field failed.");
        }
        catch
        {
            Undo.RevertAllDownToGroup(undoGroup);
            throw;
        }
        finally { Undo.CollapseUndoOperations(undoGroup); }
        Debug.Log("Authored fixed six-zone paths, low meadow clusters and wall groves. Navigation/colliders/material assets unchanged.");
    }

    [MenuItem("C02/Art/Apply Courtyard Road Approach")]
    public static void ApplyCourtyardApproach()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the saved C02 scene in Edit Mode.");
        var authors = Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        if (authors.Length != 1 || authors[0].map == null || !authors[0].map.IsValid ||
            authors[0].obstacles == null || authors[0].obstacles.Length != 18)
            throw new InvalidOperationException("The existing baked world map is required.");
        var author = authors[0];
        Transform world = author.transform.parent;
        Transform citadel = world == null ? null : world.Find("CitadelEnvironment");
        if (citadel == null || !citadel.gameObject.activeSelf || citadel.position != Vector3.zero ||
            citadel.rotation != Quaternion.identity || citadel.lossyScale != Vector3.one)
            throw new InvalidOperationException("The active, unscaled Citadel environment is required.");
        const string folder = "Assets/C02/Art/CitadelEnvironment/";
        var roadMesh = AssetDatabase.LoadAssetAtPath<Mesh>(folder + "Meadow0_WornRoadStone.asset");
        var stoneMesh = AssetDatabase.LoadAssetAtPath<Mesh>(folder + "BevelledAshlar.asset");
        var leafMesh = AssetDatabase.LoadAssetAtPath<Mesh>(folder + "CuppedOakLeaf.asset");
        var roadMaterial = AssetDatabase.LoadAssetAtPath<Material>(folder + "WornRoadStone.mat");
        var stoneMaterial = AssetDatabase.LoadAssetAtPath<Material>(folder + "WeatheredLimestone.mat");
        var leafMaterial = AssetDatabase.LoadAssetAtPath<Material>(folder + "OakLeaves.mat");
        if (roadMesh == null || stoneMesh == null || leafMesh == null || roadMaterial == null ||
            stoneMaterial == null || leafMaterial == null)
            throw new InvalidOperationException("Existing Citadel road, stone and foliage assets are required.");
        // Continue the existing eastbound cobbles (x7..53, z9..11). The exterior
        // gates are scenery beyond navigation, not a new passage or destination.
        Vector3 extension = new Vector3(46.9f, 0, 0);
        foreach (Vector3 vertex in roadMesh.vertices)
        {
            Vector3 point = vertex + extension;
            if (!author.map.IsWalkable(point.x, point.z) || NearObstacle(author, point, .5f))
                throw new InvalidOperationException("The existing road extension must remain on clear baked navigation.");
        }
        // Check the interior as well as the vertices, so no road triangle visually
        // bridges a blocked tile. Half-tile sampling covers this fixed axis-aligned strip.
        Bounds roadBounds = roadMesh.bounds;
        roadBounds.center += extension;
        float sampleStep = author.map.tileSize * .5f;
        for (float x = roadBounds.min.x; x <= roadBounds.max.x + sampleStep; x += sampleStep)
            for (float z = roadBounds.min.z; z <= roadBounds.max.z + sampleStep; z += sampleStep)
            {
                var point = new Vector3(Mathf.Min(x, roadBounds.max.x), 0, Mathf.Min(z, roadBounds.max.z));
                if (!author.map.IsWalkable(point.x, point.z) || NearObstacle(author, point, .5f))
                    throw new InvalidOperationException("The complete courtyard road strip must remain walkable.");
            }
        Transform root = citadel.Find("TownRoadApproach");
        if (root != null && root.GetComponentsInChildren<Collider>(true).Length != 0)
            throw new InvalidOperationException("The owned approach must remain presentation-only.");
        Undo.IncrementCurrentGroup();
        int undoGroup = Undo.GetCurrentGroup();
        Undo.SetCurrentGroupName("Tidy courtyard road approach");
        try
        {
            root = RoadObject(citadel, "TownRoadApproach");
            RoadMesh(root, "EastboundCobbles", roadMesh, roadMaterial, extension, Vector3.one, 0);
            for (int marker = 0; marker < 8; marker++)
            {
                float x = 56 + marker * 6;
                for (int side = -1; side <= 1; side += 2)
                {
                    string suffix = marker + "_" + side;
                    // Ankle-flat accents leave the centre and safe spawn visibly open.
                    RoadMesh(root, "RoadEdge_" + suffix, stoneMesh, stoneMaterial,
                        new Vector3(x, .025f, 10 + side * 2.25f), new Vector3(.85f, .045f, .38f), side * 4);
                    for (int leaf = 0; leaf < 3; leaf++)
                    {
                        var plant = RoadMesh(root, "LowVerge_" + suffix + "_" + leaf, leafMesh, leafMaterial,
                            new Vector3(x + leaf * .24f, .10f, 10 + side * 3.15f), new Vector3(.32f, .22f, .32f), marker * 47 + leaf * 63);
                        plant.localRotation *= Quaternion.Euler(52, 0, 0);
                    }
                }
            }
            EditorSceneManager.MarkSceneDirty(scene);
            Undo.FlushUndoRecordObjects();
            if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Saving the courtyard road failed.");
        }
        catch
        {
            Undo.RevertAllDownToGroup(undoGroup);
            throw;
        }
        finally { Undo.CollapseUndoOperations(undoGroup); }
        Debug.Log("Courtyard road tidied with existing cobbles, flat edging and low verge. Walls, banners, torches, spawn and navigation unchanged.");
    }

    [MenuItem("C02/Art/Extend Town Road To Field")]
    public static void ExtendTownRoadToField()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the clean, saved C02 scene in Edit Mode.");
        var authors = Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        if (authors.Length != 1 || authors[0].map == null || !authors[0].map.IsValid ||
            authors[0].obstacles == null || authors[0].obstacles.Length != 18)
            throw new InvalidOperationException("The existing baked world map is required.");
        var author = authors[0];
        Transform citadel = author.transform.parent == null ? null : author.transform.parent.Find("CitadelEnvironment");
        Transform root = citadel == null ? null : citadel.Find("TownRoadApproach");
        if (root == null || !citadel.gameObject.activeSelf || !root.gameObject.activeSelf || root.position != Vector3.zero ||
            root.rotation != Quaternion.identity || root.lossyScale != Vector3.one ||
            root.GetComponentsInChildren<Collider>(true).Length != 0)
            throw new InvalidOperationException("The existing unscaled, presentation-only courtyard road is required.");
        const string folder = "Assets/C02/Art/CitadelEnvironment/";
        var mesh = AssetDatabase.LoadAssetAtPath<Mesh>(folder + "Meadow0_WornRoadStone.asset");
        var material = AssetDatabase.LoadAssetAtPath<Material>(folder + "WornRoadStone.mat");
        var approach = root.Find("EastboundCobbles")?.GetComponent<MeshFilter>();
        var approachRenderer = approach == null ? null : approach.GetComponent<MeshRenderer>();
        if (mesh == null || material == null || approach == null || approach.sharedMesh != mesh ||
            approachRenderer == null || Mathf.Abs(approachRenderer.bounds.max.x - 100) > 1)
            throw new InvalidOperationException("The existing cobbled approach must end near x100 before extending it.");
        Bounds bounds = mesh.bounds;
        if (bounds.size.x < 40 || bounds.size.x > 50 || bounds.min.z < 8 || bounds.max.z > 12)
            throw new InvalidOperationException("Unexpected source road bounds; review the authored strip before extending it.");
        var scale = new Vector3(52.5f / bounds.size.x, 1, 1);
        Vector3[] vertices = mesh.vertices;
        float step = author.map.tileSize * .5f;
        // Two existing mesh instances span x100..205 without changing road width or navigation.
        for (int segment = 0; segment < 2; segment++)
        {
            float start = 100 + segment * 52.5f;
            var offset = new Vector3(start - bounds.min.x * scale.x, 0, 0);
            foreach (Vector3 vertex in vertices)
            {
                Vector3 point = Vector3.Scale(vertex, scale) + offset;
                if (!author.map.IsWalkable(point.x, point.z) || NearObstacle(author, point, .5f))
                    throw new InvalidOperationException("The extended road vertices must remain on clear baked navigation.");
            }
            for (float x = start; x <= start + 52.5f + step; x += step)
                for (float z = bounds.min.z; z <= bounds.max.z + step; z += step)
                {
                    var point = new Vector3(Mathf.Min(x, start + 52.5f), 0, Mathf.Min(z, bounds.max.z));
                    if (!author.map.IsWalkable(point.x, point.z) || NearObstacle(author, point, .5f))
                        throw new InvalidOperationException("The complete extended road strip must remain walkable.");
                }
        }
        Undo.IncrementCurrentGroup();
        int undoGroup = Undo.GetCurrentGroup();
        Undo.SetCurrentGroupName("Extend town road to field");
        try
        {
            for (int segment = 0; segment < 2; segment++)
                RoadMesh(root, "FieldboundCobbles_" + segment, mesh, material,
                    new Vector3(100 + segment * 52.5f - bounds.min.x * scale.x, 0, 0), scale, 0);
            // These two old one-zone horizons now cross the expanded playable world.
            // Keep the south/west exterior backdrop and preserve the original objects for rollback.
            foreach (string name in new[] { "DistantForestRidge0", "DistantForestRidge1" })
            {
                var ridge = author.transform.parent.Find("V04MeanderingMeadow/" + name);
                if (ridge == null) continue;
                Undo.RecordObject(ridge.gameObject, "Clear obsolete interior horizon");
                ridge.gameObject.SetActive(false);
            }
            EditorSceneManager.MarkSceneDirty(scene);
            Undo.FlushUndoRecordObjects();
            if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Saving the fieldbound road failed.");
        }
        catch
        {
            Undo.RevertAllDownToGroup(undoGroup);
            throw;
        }
        finally { Undo.CollapseUndoOperations(undoGroup); }
        Debug.Log("Town road extended visually from x100 to x205 using two existing mesh instances. Navigation, collision, spawn and AI unchanged.");
    }

    [MenuItem("C02/Art/Apply Selected Roadside Dressing")]
    public static void ApplySelectedRoadsideDressing()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the clean, saved C02 scene in Edit Mode.");
        var authors = Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        if (authors.Length != 1 || authors[0].map == null || !authors[0].map.IsValid ||
            authors[0].obstacles == null || authors[0].obstacles.Length != 18)
            throw new InvalidOperationException("The existing baked world map is required.");
        var author = authors[0];
        Transform citadel = author.transform.parent == null ? null : author.transform.parent.Find("CitadelEnvironment");
        Transform approach = citadel == null ? null : citadel.Find("TownRoadApproach");
        // The login scene deliberately keeps the world parent inactive in Edit Mode.
        if (approach == null || approach.Find("FieldboundCobbles_1") == null || !citadel.gameObject.activeSelf ||
            citadel.position != Vector3.zero || citadel.rotation != Quaternion.identity || citadel.lossyScale != Vector3.one)
            throw new InvalidOperationException("The active, unscaled Citadel and existing fieldbound cobbles are required.");
        const string folder = "Assets/C02/Art/CitadelEnvironment/";
        var stone = AssetDatabase.LoadAssetAtPath<Mesh>(folder + "BevelledAshlar.asset");
        var leaf = AssetDatabase.LoadAssetAtPath<Mesh>(folder + "CuppedOakLeaf.asset");
        var stoneMaterial = AssetDatabase.LoadAssetAtPath<Material>(folder + "WeatheredLimestone.mat");
        var foliageMaterial = AssetDatabase.LoadAssetAtPath<Material>(folder + "OakLeaves.mat");
        if (stone == null || leaf == null || stoneMaterial == null || foliageMaterial == null)
            throw new InvalidOperationException("Existing Citadel stone and foliage assets are required.");
        // Fixed outskirts of the approach and first hunting space, not runtime/camera-relative scenery.
        // Preserve the paved x100..205 / z9..11 corridor and the first encounter's open centre.
        Vector2[] clusters = {
            new Vector2(112, 6), new Vector2(112, 14), new Vector2(136, 6), new Vector2(136, 14),
            new Vector2(160, 6), new Vector2(160, 14), new Vector2(184, 6), new Vector2(184, 14),
            new Vector2(278, 2), new Vector2(293, 2), new Vector2(308, 2), new Vector2(324, 2),
            new Vector2(278, 35), new Vector2(294, 35), new Vector2(310, 35), new Vector2(324, 35)
        };
        foreach (Vector2 point in clusters)
            if (!author.map.IsWalkable(point.x, point.y) || NearObstacle(author, new Vector3(point.x, 0, point.y), 2))
                throw new InvalidOperationException("Selected roadside decoration must remain clear of baked obstacles.");
        Vector3 landmark = new Vector3(203, 0, 16);
        if (!author.map.IsWalkable(landmark.x, landmark.z) || NearObstacle(author, landmark, 2))
            throw new InvalidOperationException("The field-entry landmark must remain outside baked obstacles.");
        Transform existing = citadel.Find("SelectedRoadsideDressing");
        if (existing != null && existing.GetComponentsInChildren<Collider>(true).Length != 0)
            throw new InvalidOperationException("Selected roadside dressing must remain presentation-only.");
        Undo.IncrementCurrentGroup();
        int undoGroup = Undo.GetCurrentGroup();
        Undo.SetCurrentGroupName("Apply selected roadside dressing");
        try
        {
            Transform root = RoadObject(citadel, "SelectedRoadsideDressing");
            for (int cluster = 0; cluster < clusters.Length; cluster++)
            {
                Vector3 centre = new Vector3(clusters[cluster].x, 0, clusters[cluster].y);
                // Buried, ankle-height pieces do not imply an unexported solid wall.
                RoadMesh(root, "VergeStone_" + cluster, stone, stoneMaterial,
                    centre + new Vector3(.25f, .04f, .25f), new Vector3(1.15f, .16f, .7f), cluster * 37);
                for (int tuft = 0; tuft < 4; tuft++)
                {
                    var plant = RoadMesh(root, "VergeLeaf_" + cluster + "_" + tuft, leaf, foliageMaterial,
                        centre + new Vector3((tuft % 2) * .48f - .5f, .13f, (tuft / 2) * .43f - .4f),
                        new Vector3(.8f, .52f, .8f), cluster * 53 + tuft * 73);
                    plant.localRotation *= Quaternion.Euler(58, 0, 0);
                }
            }
            // One low cairn marks the end of town paving. No repeated direction signs or new service/portal.
            RoadMesh(root, "FieldEntryCairnBase", stone, stoneMaterial,
                landmark + Vector3.up * .09f, new Vector3(1.6f, .18f, 1.2f), 7);
            RoadMesh(root, "FieldEntryCairnLower", stone, stoneMaterial,
                landmark + Vector3.up * .34f, new Vector3(.95f, .38f, .72f), -8);
            RoadMesh(root, "FieldEntryCairnCap", stone, stoneMaterial,
                landmark + Vector3.up * .66f, new Vector3(.62f, .26f, .5f), 12);
            if (root.GetComponentsInChildren<Collider>(true).Length != 0)
                throw new InvalidOperationException("Roadside dressing must not introduce colliders.");
            EditorSceneManager.MarkSceneDirty(scene);
            Undo.FlushUndoRecordObjects();
            if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Saving selected roadside dressing failed.");
        }
        catch
        {
            Undo.RevertAllDownToGroup(undoGroup);
            throw;
        }
        finally { Undo.CollapseUndoOperations(undoGroup); }
        Debug.Log("Selected A roadside dressing applied: low town verges, first-hunt outskirts and one field-entry cairn. Existing navigation, collision, spawn and AI unchanged.");
    }

    private static Transform RoadObject(Transform parent, string name)
    {
        Transform child = parent.Find(name);
        if (child == null)
        {
            child = new GameObject(name).transform;
            Undo.RegisterCreatedObjectUndo(child.gameObject, "Tidy courtyard road approach");
            child.SetParent(parent, false);
        }
        Undo.RecordObject(child, "Tidy courtyard road approach");
        child.localPosition = Vector3.zero;
        child.localRotation = Quaternion.identity;
        child.localScale = Vector3.one;
        return child;
    }

    private static Transform RoadMesh(Transform parent, string name, Mesh mesh, Material material,
        Vector3 position, Vector3 scale, float yaw)
    {
        Transform child = RoadObject(parent, name);
        var filter = child.GetComponent<MeshFilter>();
        if (filter == null) filter = Undo.AddComponent<MeshFilter>(child.gameObject);
        var renderer = child.GetComponent<MeshRenderer>();
        if (renderer == null) renderer = Undo.AddComponent<MeshRenderer>(child.gameObject);
        Undo.RecordObject(filter, "Tidy courtyard road approach");
        Undo.RecordObject(renderer, "Tidy courtyard road approach");
        filter.sharedMesh = mesh;
        renderer.sharedMaterial = material;
        child.localPosition = position;
        child.localScale = scale;
        child.localRotation = Quaternion.Euler(0, yaw, 0);
        GameObjectUtility.SetStaticEditorFlags(child.gameObject, StaticEditorFlags.BatchingStatic);
        return child;
    }

    private static void AddTrail(Transform region, C20MapAuthoring author, Mesh mesh, Material material,
        Vector2 a, Vector2 b, int section)
    {
        Vector3[] vertices = mesh.vertices;
        Vector3 sourceStart = vertices[2], sourceEnd = vertices[vertices.Length - 3];
        Vector3 sourceDirection = sourceEnd - sourceStart;
        sourceDirection.y = 0;
        Quaternion sourceRotation = Quaternion.Inverse(Quaternion.LookRotation(sourceDirection));
        Vector3 destinationDirection = new Vector3(b.x - a.x, 0, b.y - a.y);
        Quaternion rotation = Quaternion.LookRotation(destinationDirection);
        Vector3 scale = new Vector3(1, 1, destinationDirection.magnitude / sourceDirection.magnitude);
        Vector3 start = new Vector3(a.x, 0, a.y);
        Vector3 sourceOrigin = new Vector3(sourceStart.x, 0, sourceStart.z);
        // Check the actual curved footprint, not just its straight chord.
        foreach (Vector3 vertex in vertices)
        {
            Vector3 point = region.position + start + rotation * Vector3.Scale(scale, sourceRotation * (vertex - sourceOrigin));
            if (NearObstacle(author, point, .5f)) return;
        }
        var segment = new GameObject("WornRoute_" + section).transform;
        segment.SetParent(region, false);
        segment.localPosition = start;
        segment.localRotation = rotation;
        segment.localScale = scale;
        var ribbon = new GameObject("FeatheredEarth", typeof(MeshFilter), typeof(MeshRenderer));
        ribbon.transform.SetParent(segment, false);
        ribbon.transform.localRotation = sourceRotation;
        ribbon.transform.localPosition = -(sourceRotation * sourceOrigin);
        ribbon.GetComponent<MeshFilter>().sharedMesh = mesh;
        ribbon.GetComponent<MeshRenderer>().sharedMaterial = material;
    }

    private static bool NearObstacle(C20MapAuthoring author, Vector3 point, float margin)
    {
        foreach (var obstacle in author.obstacles)
        {
            Bounds bounds = C20MapAuthoring.WorldBounds(obstacle);
            if (point.x >= bounds.min.x - margin && point.x <= bounds.max.x + margin &&
                point.z >= bounds.min.z - margin && point.z <= bounds.max.z + margin) return true;
        }
        return false;
    }

    private static GameObject Copy(Transform template, Transform parent, string name, Vector3 position, Vector3 scale, float yaw)
    {
        var copy = Object.Instantiate(template.gameObject, parent);
        copy.name = name;
        copy.SetActive(true);
        copy.transform.localPosition = position;
        copy.transform.localRotation = Quaternion.Euler(0, yaw, 0);
        copy.transform.localScale = scale;
        foreach (var collider in copy.GetComponentsInChildren<Collider>(true)) Object.DestroyImmediate(collider);
        return copy;
    }
}
