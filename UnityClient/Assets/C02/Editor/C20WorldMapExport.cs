using System;
using System.Globalization;
using System.IO;
using System.Text;
using C02;
using UnityEditor;
using UnityEngine;

public static class C20WorldMapExport
{
    [MenuItem("C02/World/Bake and Export Navigation")]
    public static void BakeAndExport()
    {
        if (EditorApplication.isPlaying) throw new InvalidOperationException("Bake the map in Edit Mode.");
        var authors = UnityEngine.Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        if (authors.Length != 1) throw new InvalidOperationException("Exactly one fixed world map authoring root is required.");
        string directory = Path.GetFullPath(Path.Combine(Application.dataPath, "../../CoreLib/Generated"));
        BakeAndExport(authors[0], directory);
        AssetDatabase.SaveAssetIfDirty(authors[0].map);
        Debug.Log("World navigation baked from explicit ground and solid objects; C++ snapshot exported. No A* or live server restart.");
    }

    // Preserve the user's previous baked array and dirty state if either step fails.
    // This is exception recovery, not a crash-atomic transaction across Unity and C++ files.
    public static void BakeAndExport(C20MapAuthoring author, string outputDirectory)
    {
        if (EditorApplication.isPlaying) throw new InvalidOperationException("Bake the map in Edit Mode.");
        if (author == null || author.map == null)
            throw new InvalidOperationException("A map authoring root and map are required.");
        var map = author.map;
        byte[] previous = map.walkable;
        bool wasDirty = EditorUtility.IsDirty(map);
        try
        {
            Bake(author);
            var layout = C24WorldLayout.Build(author.zoneColumns, author.zoneRows, author.zoneCoreSize, author.transitionMargin);
            C24WorldLayoutExport.Export(map, author, layout, outputDirectory);
        }
        catch
        {
            map.walkable = previous;
            if (wasDirty) EditorUtility.SetDirty(map);
            else EditorUtility.ClearDirty(map);
            throw;
        }
    }

    public static void Bake(C20MapAuthoring author)
    {
        if (author == null || author.map == null || author.ground == null || author.obstacles == null ||
            !Finite(author.actorClearance) || author.actorClearance < 0)
            throw new InvalidOperationException("A map, ground, explicit obstacle list and finite clearance are required.");
        if (author.transform.position != Vector3.zero ||
            (author.transform.TransformVector(Vector3.right) - Vector3.right).sqrMagnitude > .00000001f ||
            (author.transform.TransformVector(Vector3.up) - Vector3.up).sqrMagnitude > .00000001f ||
            (author.transform.TransformVector(Vector3.forward) - Vector3.forward).sqrMagnitude > .00000001f)
            throw new InvalidOperationException("The fixed authoring root must be at world origin with identity rotation/scale.");
        Vector3 groundX = author.ground.transform.TransformVector(Vector3.right);
        Vector3 groundY = author.ground.transform.TransformVector(Vector3.up);
        Vector3 groundZ = author.ground.transform.TransformVector(Vector3.forward);
        if (!author.ground.transform.IsChildOf(author.transform) ||
            Mathf.Abs(groundX.y) + Mathf.Abs(groundX.z) + Mathf.Abs(groundY.x) + Mathf.Abs(groundY.z) +
            Mathf.Abs(groundZ.x) + Mathf.Abs(groundZ.y) > .0001f)
            throw new InvalidOperationException("The ground must be an axis-aligned child of the fixed authoring root.");
        var map = author.map;
        if (map.width < 1 || map.height < 1 || map.width > 4096 || map.height > 4096 ||
            !Finite(map.origin.x) || !Finite(map.origin.y) || !Finite(map.tileSize) || map.tileSize <= 0)
            throw new InvalidOperationException("Invalid map bounds or tile size.");
        Bounds ground = Footprint(author.ground);
        float maxX = map.origin.x + map.width * map.tileSize, maxY = map.origin.y + map.height * map.tileSize;
        if (!Finite(maxX) || !Finite(maxY) || Math.Abs(ground.min.x - map.origin.x) > .001 ||
            Math.Abs(ground.min.z - map.origin.y) > .001 || Math.Abs(ground.max.x - maxX) > .001 || Math.Abs(ground.max.z - maxY) > .001)
            throw new InvalidOperationException("Ground footprint must match the fixed grid bounds.");
        var blocked = new Bounds[author.obstacles.Length];
        for (int i = 0; i < blocked.Length; i++)
        {
            if (author.obstacles[i] == null || author.obstacles[i] == author.ground ||
                !author.obstacles[i].transform.IsChildOf(author.transform))
                throw new InvalidOperationException("Every solid must be an explicit child collider of the fixed authoring root.");
            blocked[i] = Footprint(author.obstacles[i]);
        }
        var cells = new byte[checked(map.width * map.height)];
        float inset = map.tileSize * .5f + author.actorClearance;
        int legal = 0;
        for (int y = 0; y < map.height; y++)
            for (int x = 0; x < map.width; x++)
            {
                float px = map.origin.x + (x + .5f) * map.tileSize, py = map.origin.y + (y + .5f) * map.tileSize;
                bool pass = px - inset >= ground.min.x && px + inset <= ground.max.x &&
                    py - inset >= ground.min.z && py + inset <= ground.max.z;
                foreach (var box in blocked)
                    if (px + inset >= box.min.x && px - inset <= box.max.x && py + inset >= box.min.z && py - inset <= box.max.z)
                    { pass = false; break; }
                cells[y * map.width + x] = pass ? (byte)1 : (byte)0;
                if (pass) legal++;
            }
        if (legal == 0) throw new InvalidOperationException("The authored map has no walkable tiles.");
        // Build the complete snapshot before replacing the asset's data.
        map.walkable = cells;
        EditorUtility.SetDirty(map);
    }

    public static void Export(C20WorldMap map, string outputDirectory)
    {
        if (map == null || !map.IsValid) throw new InvalidOperationException("Bake a valid map before export.");
        ExportSnapshot(map, null, null, outputDirectory);
    }

    internal static void ExportSnapshot(C20WorldMap map, C20MapAuthoring author, C24WorldLayout layout, string outputDirectory)
    {
        var bits = new ulong[(map.walkable.Length + 63) / 64];
        bool any = false;
        for (int i = 0; i < map.walkable.Length; i++)
        {
            if (map.walkable[i] > 1) throw new InvalidOperationException("Walkability values must be zero or one.");
            if (map.walkable[i] == 1) { bits[i / 64] |= 1UL << (i % 64); any = true; }
        }
        if (!any) throw new InvalidOperationException("An empty navigation map cannot be exported.");
        var text = new StringBuilder("// Generated by C20WorldMapExport from the fixed Unity map. Do not edit.\n");
        text.AppendFormat(CultureInfo.InvariantCulture,
            "inline constexpr float OriginX = {0:0.0########}f;\ninline constexpr float OriginY = {1:0.0########}f;\ninline constexpr float TileSize = {2:0.0########}f;\ninline constexpr int Width = {3};\ninline constexpr int Height = {4};\n",
            map.origin.x, map.origin.y, map.tileSize, map.width, map.height);
        text.Append("inline constexpr std::array<uint64_t, (Width * Height + 63) / 64> WalkableBits = {\n");
        for (int i = 0; i < bits.Length; i++)
        {
            text.Append("0x").Append(bits[i].ToString("X16", CultureInfo.InvariantCulture)).Append("ULL,");
            if (i % 6 == 5) text.Append('\n');
        }
        text.Append("\n};\n");
        if (author != null) C24WorldLayoutExport.AppendMetadata(text, author, layout);
        Directory.CreateDirectory(outputDirectory);
        string destination = Path.Combine(outputDirectory, "WorldMap.inc");
        if (File.Exists(destination) && (File.GetAttributes(destination) & FileAttributes.ReadOnly) != 0)
            throw new UnauthorizedAccessException("The navigation export is read-only; the previous snapshot was preserved.");
        string temporary = Path.Combine(outputDirectory, ".WorldMap-" + Guid.NewGuid().ToString("N") + ".tmp");
        try
        {
            File.WriteAllText(temporary, text.ToString(), new UTF8Encoding(false));
            if (File.Exists(destination)) File.Replace(temporary, destination, null);
            else File.Move(temporary, destination);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }

    private static Bounds Footprint(BoxCollider collider)
    {
        Vector3 half = collider.size * .5f;
        var bounds = new Bounds(collider.transform.TransformPoint(collider.center - half), Vector3.zero);
        for (int i = 0; i < 8; i++)
            bounds.Encapsulate(collider.transform.TransformPoint(collider.center + Vector3.Scale(half,
                new Vector3((i & 1) == 0 ? -1 : 1, (i & 2) == 0 ? -1 : 1, (i & 4) == 0 ? -1 : 1))));
        if (!Finite(bounds.min.x) || !Finite(bounds.max.x) || !Finite(bounds.min.z) || !Finite(bounds.max.z) ||
            bounds.size.x <= 0 || bounds.size.z <= 0)
            throw new InvalidOperationException("Solid/ground footprint must be finite and nonzero.");
        return bounds;
    }

    private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
}
