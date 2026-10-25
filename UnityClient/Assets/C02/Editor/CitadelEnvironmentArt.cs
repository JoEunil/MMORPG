using System;
using System.Collections.Generic;
using System.IO;
using C02;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using Object = UnityEngine.Object;

// Original modular geometry. Walls retain the exact existing collision footprints;
// exterior silhouettes sit beyond the exported ground, while meadow detail is ankle-high.
public static class CitadelEnvironmentArt
{
    private const string Folder="Assets/C02/Art/CitadelEnvironment/";
    private const string RootName="CitadelEnvironment";
    private static Material stone,trim,iron,wood,cloth,leaf,canopy,earth,fire;
    private static Mesh block,shaft,leafMesh,canopyCard;

    [MenuItem("Tools/Client Art/Apply Citadel Environment")]
    public static void Apply()
    {
        var scene=EditorSceneManager.GetActiveScene();
        if(EditorApplication.isPlayingOrWillChangePlaymode || scene.path!="Assets/C02/C02.unity")
            throw new InvalidOperationException("Open C02 in Edit Mode before applying environment art.");
        var authors=Object.FindObjectsByType<C20MapAuthoring>(FindObjectsInactive.Include,FindObjectsSortMode.None);
        if(authors.Length!=1 || authors[0].ground==null || authors[0].obstacles==null || authors[0].obstacles.Length!=18)
            throw new InvalidOperationException("The existing six-zone map and its 18 collision obstacles are required.");
        var author=authors[0];var world=author.transform.parent;
        if(world==null || world.position!=Vector3.zero || world.rotation!=Quaternion.identity || world.lossyScale!=Vector3.one || author.zoneColumns!=3 || author.zoneRows!=2)
            throw new InvalidOperationException("The existing world must remain unscaled at its original origin.");
        foreach(var obstacle in author.obstacles)
        {
            if(obstacle==null)throw new InvalidOperationException("Missing map obstacle.");
            Bounds bounds=C20MapAuthoring.WorldBounds(obstacle);
            if(Mathf.Max(bounds.size.x,bounds.size.z)<9 || Mathf.Min(bounds.size.x,bounds.size.z)<1.9f)
                throw new InvalidOperationException("Unexpected wall footprint; review its art fit first.");
        }
        PrepareAssets();
        Transform old=world.Find(RootName);
        if(old!=null)Object.DestroyImmediate(old.gameObject);
        var root=new GameObject(RootName).transform;root.SetParent(world,false);
        // Preserve the original objects for rollback, and retain all authored colliders.
        foreach(string legacy in new[]{"V01Outpost","C27FixedFieldDressing"})
            if(world.Find(legacy)!=null)world.Find(legacy).gameObject.SetActive(false);
        for(int i=0;i<author.obstacles.Length;i++)
        {
            var bounds=C20MapAuthoring.WorldBounds(author.obstacles[i]);
            bool xLong=bounds.size.x>bounds.size.z;
            float length=xLong?bounds.size.x:bounds.size.z, depth=xLong?bounds.size.z:bounds.size.x;
            var batch=new Batch("Masonry"+i,root,new Vector3(bounds.center.x,0,bounds.center.z),xLong?0:90);
            Wall(batch,length-.14f,depth-.14f,i%3==0,i);
            batch.Flush();
            var existing=author.obstacles[i].GetComponent<MeshRenderer>();if(existing!=null)existing.enabled=false;
        }
        Bounds ground=C20MapAuthoring.WorldBounds(author.ground);
        // Town vista beyond the western world limit. No solid geometry is added to navigation.
        for(int section=0;section<6;section++)
        {
            var batch=new Batch("WesternCitadel"+section,root,new Vector3(ground.min.x-2.3f,0,section*10+5),90);
            Wall(batch,9.9f,2.4f,section==2,section+61);batch.Flush();
        }
        for(int section=0;section<5;section++)
        {
            var batch=new Batch("SouthernCitadel"+section,root,new Vector3(5+section*10,0,ground.min.z-2.3f),0);
            Wall(batch,9.9f,2.4f,section==1,section+87);batch.Flush();
        }
        for(int i=0;i<8;i++)
        {
            var batch=new Batch("BoundaryOak"+i,root,new Vector3(ground.min.x-6.8f,0,5+i*18),i*39);
            Oak(batch,i+17);batch.Flush();
        }
        for(int zone=0;zone<6;zone++) Meadow(root,author,zone);
        var groundRenderer=author.ground.GetComponent<MeshRenderer>();
        if(groundRenderer!=null)
        {
            var groundMaterial=MaterialAsset("CitadelMeadow",Color.white,0,.12f,"meadow-ground.png");
            ConfigureGroundSurface(groundMaterial);
            if(groundMaterial.mainTexture==null)throw new FileNotFoundException("Import the meadow ground material source.");
            groundMaterial.mainTextureScale=new Vector2(ground.size.x/4f,ground.size.z/4f);
            groundRenderer.sharedMaterial=groundMaterial;
        }
        if(root.GetComponentsInChildren<Collider>(true).Length!=0)
            throw new InvalidOperationException("Presentation art must never introduce collision components.");
        Lighting(world);
        AssetDatabase.SaveAssets();EditorSceneManager.MarkSceneDirty(scene);
        if(!EditorSceneManager.SaveScene(scene))throw new IOException("Could not save the environment scene.");
        Debug.Log("Citadel art: 18 collision-aligned masonry modules, exterior town wall/oaks and six meadow detail chunks. Navigation/export/server data unchanged.");
    }

    [MenuItem("Tools/Client Art/Refresh Ground Surface Only")]
    public static void ApplyGroundSurface()
    {
        if(EditorApplication.isPlayingOrWillChangePlaymode)
            throw new InvalidOperationException("Ground surface authoring requires Edit Mode.");
        var material=AssetDatabase.LoadAssetAtPath<Material>(Folder+"CitadelMeadow.mat");
        if(material==null)throw new FileNotFoundException("The existing CitadelMeadow material is required.");
        ConfigureGroundSurface(material);
        var author=Object.FindFirstObjectByType<C20MapAuthoring>(FindObjectsInactive.Include);
        if(author==null || author.ground==null)throw new InvalidOperationException("The authored world ground is required.");
        var renderer=author.ground.GetComponent<MeshRenderer>();
        if(renderer==null)throw new InvalidOperationException("The authored world ground renderer is required.");
        renderer.sharedMaterial=material;
        renderer.enabled=true;
        // The old single-zone cosmetic floor sits above the exported world surface and hides its texture.
        var legacy=author.transform.parent.Find("V04MeanderingMeadow/ContinuousWalkableMeadow");
        if(legacy!=null && legacy.TryGetComponent<MeshRenderer>(out var legacyRenderer))legacyRenderer.enabled=false;
        EditorSceneManager.MarkSceneDirty(author.gameObject.scene);
        AssetDatabase.SaveAssets();
        Debug.Log("Ground surface refreshed; obsolete cosmetic floor hidden. Existing texture scale, collision geometry, lighting and navigation are unchanged.");
    }

    private static void ConfigureGroundSurface(Material material)
    {
        var albedo=ImportGroundTexture("meadow-ground.png",false);
        var normal=ImportGroundTexture("meadow-ground-normal.png",true);
        material.SetTexture("_BaseMap",albedo);
        material.SetTexture("_MainTex",albedo);
        material.SetTexture("_BumpMap",normal);
        material.SetFloat("_BumpScale",.25f);
        material.SetFloat("_Smoothness",.12f);
        material.EnableKeyword("_NORMALMAP");
        EditorUtility.SetDirty(material);
    }

    [MenuItem("Tools/Client Art/Refresh Road And Verge Materials Only")]
    public static void RefreshRoadAndVergeMaterials()
    {
        var scene=EditorSceneManager.GetActiveScene();
        if(EditorApplication.isPlayingOrWillChangePlaymode || scene.path!="Assets/C02/C02.unity" || scene.isDirty)
            throw new InvalidOperationException("Open the clean, saved C02 scene in Edit Mode before refreshing road materials.");
        var road=AssetDatabase.LoadAssetAtPath<Material>(Folder+"WornRoadStone.mat");
        var verge=AssetDatabase.LoadAssetAtPath<Material>(Folder+"OakLeaves.mat");
        if(road==null || verge==null)
            throw new FileNotFoundException("The existing road and low-verge materials are required.");
        // The limestone albedo is already shaded. A second dark tint hides its grain.
        road.color=new Color(.52f,.49f,.40f);
        road.SetFloat("_Smoothness",.12f);
        verge.color=new Color(.31f,.39f,.17f);
        verge.SetFloat("_Smoothness",.12f);
        EditorUtility.SetDirty(road);
        EditorUtility.SetDirty(verge);
        AssetDatabase.SaveAssets();
        Debug.Log("Existing road and low-verge material tints refreshed. Geometry, placement, lighting and navigation are unchanged.");
    }

    private static Texture2D ImportGroundTexture(string name,bool normal)
    {
        string path=Folder+name;
        var importer=AssetImporter.GetAtPath(path) as TextureImporter;
        if(importer==null)throw new FileNotFoundException("Import the ground texture source before refreshing: "+path);
        importer.textureType=normal?TextureImporterType.NormalMap:TextureImporterType.Default;
        importer.convertToNormalmap=false;
        importer.sRGBTexture=!normal;
        importer.mipmapEnabled=true;
        importer.wrapMode=TextureWrapMode.Repeat;
        importer.filterMode=FilterMode.Trilinear;
        importer.anisoLevel=8;
        importer.maxTextureSize=2048;
        importer.npotScale=TextureImporterNPOTScale.None;
        importer.textureCompression=TextureImporterCompression.CompressedHQ;
        importer.compressionQuality=100;
        importer.SaveAndReimport();
        return AssetDatabase.LoadAssetAtPath<Texture2D>(path);
    }

    private static void Lighting(Transform world)
    {
        var urp=UnityEngine.Rendering.Universal.UniversalRenderPipeline.asset;
        if(urp==null)throw new InvalidOperationException("Citadel lighting requires the project's existing URP pipeline.");
        urp.supportsHDR=true;EditorUtility.SetDirty(urp);
        RenderSettings.ambientMode=AmbientMode.Trilight;
        RenderSettings.ambientSkyColor=new Color(.72f,.80f,.92f);
        RenderSettings.ambientEquatorColor=new Color(.60f,.63f,.66f);
        RenderSettings.ambientGroundColor=new Color(.36f,.34f,.31f);
        RenderSettings.fog=true;RenderSettings.fogMode=FogMode.ExponentialSquared;
        RenderSettings.fogColor=new Color(.50f,.54f,.56f);RenderSettings.fogDensity=.006f;
        var sun=world.Find("FieldLight")?.GetComponent<Light>();
        if(sun!=null){sun.color=new Color(1f,.93f,.83f);sun.intensity=1.45f;sun.transform.rotation=Quaternion.Euler(43,-38,0);sun.shadows=LightShadows.Soft;sun.shadowBias=.04f;}
        const string skyPath=Folder+"CitadelReflection.asset";
        var sky=AssetDatabase.LoadAssetAtPath<Cubemap>(skyPath);
        if(sky==null)
        {
            const int size=32;sky=new Cubemap(size,TextureFormat.RGBAHalf,true){name="CitadelReflection"};
            var key=new Vector3(-.5f,.65f,-.4f).normalized;
            for(int face=0;face<6;face++)
            {
                var pixels=new Color[size*size];
                for(int y=0;y<size;y++)for(int x=0;x<size;x++)
                {
                    float u=(x+.5f)/size*2-1,v=(y+.5f)/size*2-1;
                    Vector3 d=face==0?new Vector3(1,v,-u):face==1?new Vector3(-1,v,u):face==2?new Vector3(u,1,-v):face==3?new Vector3(u,-1,v):face==4?new Vector3(u,v,1):new Vector3(-u,v,-1);
                    d.Normalize();float light=Mathf.Pow(Mathf.Max(0,Vector3.Dot(d,key)),40)*3;
                    pixels[y*size+x]=Color.Lerp(new Color(.12f,.105f,.09f),new Color(.45f,.56f,.72f),d.y*.5f+.5f)+new Color(light,light*.73f,light*.42f,0);
                }
                sky.SetPixels(pixels,(CubemapFace)face);
            }
            sky.Apply(true);AssetDatabase.CreateAsset(sky,skyPath);
        }
        RenderSettings.defaultReflectionMode=DefaultReflectionMode.Custom;RenderSettings.customReflection=sky;RenderSettings.reflectionIntensity=1.1f;
        DynamicGI.UpdateEnvironment();
        const string profilePath=Folder+"CitadelGrade.asset";
        var profile=AssetDatabase.LoadAssetAtPath<VolumeProfile>(profilePath);
        if(profile==null){profile=ScriptableObject.CreateInstance<VolumeProfile>();AssetDatabase.CreateAsset(profile,profilePath);}
        if(!profile.TryGet<UnityEngine.Rendering.Universal.Tonemapping>(out var tone)){tone=profile.Add<UnityEngine.Rendering.Universal.Tonemapping>(true);AssetDatabase.AddObjectToAsset(tone,profile);}
        tone.mode.Override(UnityEngine.Rendering.Universal.TonemappingMode.ACES);
        if(!profile.TryGet<UnityEngine.Rendering.Universal.ColorAdjustments>(out var grade)){grade=profile.Add<UnityEngine.Rendering.Universal.ColorAdjustments>(true);AssetDatabase.AddObjectToAsset(grade,profile);}
        grade.postExposure.Override(.55f);grade.contrast.Override(12);grade.saturation.Override(-8);
        if(!profile.TryGet<UnityEngine.Rendering.Universal.Bloom>(out var bloom)){bloom=profile.Add<UnityEngine.Rendering.Universal.Bloom>(true);AssetDatabase.AddObjectToAsset(bloom,profile);}
        bloom.threshold.Override(1.15f);bloom.intensity.Override(.12f);bloom.scatter.Override(.5f);
        var existing=world.Find("CitadelLighting");
        var volume=existing!=null?existing.GetComponent<Volume>():new GameObject("CitadelLighting",typeof(Volume)).GetComponent<Volume>();
        volume.transform.SetParent(world,false);volume.isGlobal=true;volume.priority=3;volume.sharedProfile=profile;
        foreach(var camera in world.GetComponentsInChildren<Camera>(true))
        {
            var data=camera.GetComponent<UnityEngine.Rendering.Universal.UniversalAdditionalCameraData>()??camera.gameObject.AddComponent<UnityEngine.Rendering.Universal.UniversalAdditionalCameraData>();
            camera.allowHDR=true;data.renderPostProcessing=true;data.volumeLayerMask=1;
        }
        EditorUtility.SetDirty(profile);
    }

    private static void Wall(Batch batch,float length,float depth,bool gate,int seed)
    {
        var random=new System.Random(seed*31+121);
        // Offset ashlar courses, inset mortar joints and separately bevelled coping stones.
        int columns=Mathf.CeilToInt(length/1.05f);float width=length/columns;
        for(int row=0;row<9;row++)for(int col=0;col<=columns;col++)
        {
            float left=-length*.5f+col*width;
            if(row%2==1)left-=width*.5f;
            float right=Mathf.Min(left+width,length*.5f);left=Mathf.Max(left,-length*.5f);
            if(right-left<.12f)continue;
            float x=(left+right)*.5f;
            if(gate && Mathf.Abs(x)<1.55f && row<6)continue;
            float shade=(float)random.NextDouble();
            batch.Box(shade<.2f?trim:stone,new Vector3(x,.23f+row*.48f,0),new Vector3(right-left-.025f,.453f,depth*.66f),Quaternion.Euler(0,(shade-.5f)*.3f,0));
        }
        batch.Box(trim,new Vector3(0,4.44f,0),new Vector3(length,.20f,depth*.80f));
        // Two layered buttresses lie inside the existing blocked rectangle.
        for(int s=-1;s<=1;s+=2)
        {
            float x=s*(length*.5f-.72f);
            batch.Box(stone,new Vector3(x,1.45f,0),new Vector3(1.27f,2.9f,depth*.94f));
            batch.Box(trim,new Vector3(x,2.98f,0),new Vector3(1.18f,.21f,depth*.97f));
            batch.Box(stone,new Vector3(x,4.02f,0),new Vector3(.98f,1.86f,depth*.85f));
            batch.Box(trim,new Vector3(x,5.02f,0),new Vector3(1.28f,.20f,depth*.96f));
            for(int side=-1;side<=1;side+=2)
                batch.Box(stone,new Vector3(x+side*.39f,5.43f,0),new Vector3(.40f,.65f,depth*.86f));
        }
        int merlons=Mathf.Max(3,Mathf.RoundToInt(length/1.25f));
        for(int i=0;i<merlons;i++)batch.Box(stone,new Vector3(Mathf.Lerp(-length*.5f+.36f,length*.5f-.36f,i/(float)(merlons-1)),4.91f,0),new Vector3(.68f,.76f,depth*.74f));
        if(gate)
        {
            // The iron grille is CLOSED: the source collider blocks this entire footprint.
            for(int i=0;i<9;i++)batch.Box(iron,new Vector3(-1.22f+i*.305f,1.44f,-depth*.36f),new Vector3(.062f,2.86f,.07f));
            for(int i=0;i<4;i++)batch.Box(iron,new Vector3(0,.35f+i*.67f,-depth*.37f),new Vector3(2.53f,.07f,.10f));
            for(int i=0;i<9;i++)
            {
                float a=Mathf.PI*i/8;
                batch.Box(trim,new Vector3(Mathf.Cos(a)*1.48f,2.60f+Mathf.Sin(a)*.84f,-depth*.03f),new Vector3(.46f,.40f,depth*.83f),Quaternion.Euler(0,0,a*Mathf.Rad2Deg-90));
            }
        }
        for(int s=-1;s<=1;s+=2)
        {
            float x=s*(length*.5f-.72f);
            // Include the furthest forward vertices, not only the attachment origin.
            // Banner relief reaches 0.127 m; torch head/bracket reaches <0.37 m.
            // Both are inset from the module face, itself inside the original collider.
            Banner(batch,new Vector3(x,4.37f,-depth*.5f+.20f),seed+s);
            Torch(batch,new Vector3(x,1.72f,-depth*.5f+.40f));
        }
    }

    private static void Banner(Batch batch,Vector3 center,int seed)
    {
        batch.Box(iron,center+new Vector3(0,.14f,0),new Vector3(.93f,.045f,.055f));
        var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();
        for(int row=0;row<=10;row++)for(int col=0;col<=6;col++)
        {
            float u=col/6f,f=row/10f;
            float bottom=1.68f-(Mathf.Abs(u-.5f)*.3f);
            v.Add(center+new Vector3((u-.5f)*.8f,-f*bottom,-.04f-Mathf.Sin(u*Mathf.PI*6+seed)*(.016f+f*.045f)));
            uv.Add(new Vector2(u,f));
        }
        for(int r=0;r<10;r++)for(int c=0;c<6;c++){int a=r*7+c;t.AddRange(new[]{a,a+1,a+7,a+1,a+8,a+7});}
        batch.AddMesh(TemporaryMesh(v,t,uv),cloth,Matrix4x4.identity,true);
        // Raised narrow metal thread motif, readable from the quarter view without text.
        batch.Box(trim,center+new Vector3(0,-.56f,-.115f),new Vector3(.045f,.58f,.014f));
        batch.Box(trim,center+new Vector3(0,-.40f,-.12f),new Vector3(.30f,.04f,.014f));
    }

    private static void Torch(Batch batch,Vector3 p)
    {
        batch.Box(iron,p+new Vector3(0,0,-.10f),new Vector3(.15f,.32f,.22f));
        batch.AddMesh(shaft,wood,Matrix4x4.TRS(p+new Vector3(0,.2f,-.23f),Quaternion.Euler(-15,0,0),new Vector3(.06f,.51f,.06f)));
        batch.AddMesh(shaft,iron,Matrix4x4.TRS(p+new Vector3(0,.50f,-.3f),Quaternion.identity,new Vector3(.11f,.15f,.11f)));
        batch.AddMesh(leafMesh,fire,Matrix4x4.TRS(p+new Vector3(0,.69f,-.3f),Quaternion.Euler(0,17,0),new Vector3(.13f,.28f,.13f)));
    }

    private static void Oak(Batch batch,int seed)
    {
        var random=new System.Random(seed);float height=6.8f+(float)random.NextDouble()*1.4f;
        batch.AddMesh(shaft,wood,Matrix4x4.TRS(new Vector3(0,height*.5f,0),Quaternion.Euler(2,0,-4),new Vector3(.48f,height,.48f)));
        for(int limb=0;limb<6;limb++)
        {
            float angle=limb*2.4f+seed;
            var end=new Vector3(Mathf.Cos(angle)*2.25f,height*.70f+limb*.22f,Mathf.Sin(angle)*2.25f);
            Vector3 start=new Vector3(0,height*.48f,0),delta=end-start;
            batch.AddMesh(shaft,wood,Matrix4x4.TRS((start+end)*.5f,Quaternion.FromToRotation(Vector3.up,delta),new Vector3(.19f,delta.magnitude,.19f)));
            // Two crossed leaf clusters per limb give twelve irregular cutout clusters per oak.
            // The cards remain static geometry, not camera-facing runtime objects.
            for(int i=0;i<2;i++)
            {
                float yaw=limb*137+i*90;
                Vector3 p=end+new Vector3(i*.16f,.15f,0);
                batch.AddMesh(canopyCard,canopy,Matrix4x4.TRS(p,Quaternion.Euler(i==0?32:58,yaw,(limb%3-1)*11),new Vector3(3.4f,2.9f,1)));
            }
        }
    }

    private static void Meadow(Transform root,C20MapAuthoring author,int zone)
    {
        Vector3 origin=new Vector3(zone%3*200,0,zone/3*200);
        var batch=new Batch("Meadow"+zone,root,origin,0);var random=new System.Random(9137+zone*71);
        for(int i=0;i<420;i++)
        {
            var p=new Vector3(4+(float)random.NextDouble()*192,0,4+(float)random.NextDouble()*192);
            // Entrance and primary eastbound road stay open and visually quiet.
            if(zone==0 && p.x<30 && p.z<25)continue;
            if(p.z<15 && p.x<55)continue;
            bool blocked=false;
            foreach(var obstacle in author.obstacles){Bounds b=C20MapAuthoring.WorldBounds(obstacle);Vector3 w=p+origin;if(w.x>b.min.x-1 && w.x<b.max.x+1 && w.z>b.min.z-1 && w.z<b.max.z+1){blocked=true;break;}}
            if(blocked)continue;
            float yaw=(float)random.NextDouble()*360;
            for(int stem=0;stem<5;stem++)batch.AddMesh(leafMesh,leaf,Matrix4x4.TRS(p+new Vector3(stem*.14f-.28f,.07f,Mathf.Sin(stem*2f)*.18f),Quaternion.Euler(52,yaw+stem*72,0),new Vector3(.16f,.30f,.16f)));
            if(i%5==0)batch.Box(stone,p+Vector3.up*.025f,new Vector3(.55f,.13f,.38f),Quaternion.Euler(0,yaw,4));
        }
        // Worn cobbles are inset into the existing ground; no new collision or path contract.
        if(zone==0)for(int i=0;i<70;i++)
        {
            float x=7+i*.67f;
            for(int lane=0;lane<3;lane++)batch.Box(earth,new Vector3(x,.012f,9.2f+lane*.64f+Mathf.Sin(i*.4f)*.22f),new Vector3(.58f,.035f,.52f),Quaternion.Euler(0,(i%3-1)*7,0));
        }
        batch.Flush();
    }

    private sealed class Batch
    {
        private readonly string name;private readonly Transform parent;private readonly Vector3 position;private readonly Quaternion rotation;
        private readonly Dictionary<Material,List<CombineInstance>> parts=new Dictionary<Material,List<CombineInstance>>();
        private readonly List<Mesh> temporary=new List<Mesh>();
        public Batch(string name,Transform parent,Vector3 position,float yaw){this.name=name;this.parent=parent;this.position=position;rotation=Quaternion.Euler(0,yaw,0);}
        public void Box(Material material,Vector3 p,Vector3 size){Box(material,p,size,Quaternion.identity);}
        public void Box(Material material,Vector3 p,Vector3 size,Quaternion q){AddMesh(block,material,Matrix4x4.TRS(p,q,size));}
        public void AddMesh(Mesh mesh,Material material,Matrix4x4 matrix,bool release=false)
        {if(!parts.TryGetValue(material,out var list)){list=new List<CombineInstance>();parts.Add(material,list);}list.Add(new CombineInstance{mesh=mesh,transform=matrix});if(release)temporary.Add(mesh);}
        public void Flush()
        {
            var chunk=new GameObject(name).transform;chunk.SetParent(parent,false);chunk.localPosition=position;chunk.localRotation=rotation;
            foreach(var pair in parts)
            {
                string path=Folder+name+"_"+pair.Key.name+".asset";var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(path);
                if(mesh==null){mesh=new Mesh{name=name+"_"+pair.Key.name,indexFormat=IndexFormat.UInt32};AssetDatabase.CreateAsset(mesh,path);}else mesh.Clear();
                mesh.indexFormat=IndexFormat.UInt32;mesh.CombineMeshes(pair.Value.ToArray(),true,true);mesh.RecalculateBounds();EditorUtility.SetDirty(mesh);
                var go=new GameObject(pair.Key.name,typeof(MeshFilter),typeof(MeshRenderer));go.transform.SetParent(chunk,false);
                go.GetComponent<MeshFilter>().sharedMesh=mesh;go.GetComponent<MeshRenderer>().sharedMaterial=pair.Key;
                GameObjectUtility.SetStaticEditorFlags(go,StaticEditorFlags.BatchingStatic);
            }
            foreach(var mesh in temporary)Object.DestroyImmediate(mesh);
        }
    }

    private static void PrepareAssets()
    {
        if(!AssetDatabase.IsValidFolder(Folder.TrimEnd('/'))){Directory.CreateDirectory(Folder);AssetDatabase.Refresh();}
        stone=MaterialAsset("WeatheredLimestone",new Color(.57f,.55f,.48f),0,.23f,"limestone-surface.png");
        trim=MaterialAsset("CutSandstone",new Color(.74f,.67f,.51f),0,.30f,"limestone-surface.png");
        iron=MaterialAsset("ForgedIron",new Color(.085f,.096f,.105f),.76f,.35f,null);
        wood=MaterialAsset("AgedOak",new Color(.27f,.20f,.13f),0,.17f,"oak-bark.png");
        cloth=MaterialAsset("HeraldicBurgundy",new Color(.25f,.025f,.041f),0,.16f,null);
        leaf=MaterialAsset("OakLeaves",new Color(.31f,.39f,.17f),0,.12f,null);
        canopy=MaterialAsset("OakCanopyCutout",new Color(.75f,.80f,.60f),0,.15f,"oak-leaf-cluster.png");
        if(canopy.mainTexture==null)throw new FileNotFoundException("Import the authored transparent oak leaf cluster before applying citadel art.");
        canopy.SetFloat("_AlphaClip",1);canopy.SetFloat("_Cutoff",.32f);canopy.SetFloat("_Surface",0);canopy.SetFloat("_ZWrite",1);
        canopy.EnableKeyword("_ALPHATEST_ON");canopy.SetOverrideTag("RenderType","TransparentCutout");canopy.renderQueue=(int)RenderQueue.AlphaTest;EditorUtility.SetDirty(canopy);
        earth=MaterialAsset("WornRoadStone",new Color(.52f,.49f,.40f),0,.12f,"limestone-surface.png");
        fire=MaterialAsset("TorchAmber",new Color(1f,.37f,.065f),0,.5f,null);fire.EnableKeyword("_EMISSION");fire.SetColor("_EmissionColor",new Color(3.5f,.65f,.06f));EditorUtility.SetDirty(fire);
        block=BevelBlock();shaft=BranchMesh();leafMesh=LeafMesh();
        canopyCard=StoreMesh("CanopyClusterCard",new List<Vector3>{new Vector3(-.5f,-.5f,0),new Vector3(.5f,-.5f,0),new Vector3(.5f,.5f,0),new Vector3(-.5f,.5f,0)},new List<int>{0,2,1,0,3,2},new List<Vector2>{Vector2.zero,Vector2.right,Vector2.one,Vector2.up});
    }

    private static Material MaterialAsset(string name,Color color,float metallic,float smoothness,string textureName)
    {
        string path=Folder+name+".mat";var material=AssetDatabase.LoadAssetAtPath<Material>(path);
        if(material==null)
        {var shader=Shader.Find("Universal Render Pipeline/Lit");if(shader==null)throw new InvalidOperationException("URP Lit is required.");material=new Material(shader){name=name};AssetDatabase.CreateAsset(material,path);}
        material.color=color;material.SetFloat("_Metallic",metallic);material.SetFloat("_Smoothness",smoothness);material.SetFloat("_Cull",0);
        if(textureName!=null){var texture=AssetDatabase.LoadAssetAtPath<Texture2D>(Folder+textureName);if(texture!=null)material.mainTexture=texture;}
        EditorUtility.SetDirty(material);return material;
    }

    private static Mesh BevelBlock()
    {
        var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();const float h=.5f,k=.44f;
        void Face(params Vector3[] points)
        {
            Vector3 center=Vector3.zero;foreach(var p in points)center+=p;center/=points.Length;
            bool flip=Vector3.Dot(Vector3.Cross(points[1]-points[0],points[2]-points[0]),center)<0;
            int start=v.Count;for(int i=0;i<points.Length;i++){v.Add(points[i]);uv.Add(new Vector2((i==1||i==2)?1:0,i>=2?1:0));}
            for(int i=1;i<points.Length-1;i++)if(flip)t.AddRange(new[]{start,start+i+1,start+i});else t.AddRange(new[]{start,start+i,start+i+1});
        }
        for(int s=-1;s<=1;s+=2)
        {
            Face(new Vector3(s*h,-k,-k),new Vector3(s*h,k,-k),new Vector3(s*h,k,k),new Vector3(s*h,-k,k));
            Face(new Vector3(-k,s*h,-k),new Vector3(k,s*h,-k),new Vector3(k,s*h,k),new Vector3(-k,s*h,k));
            Face(new Vector3(-k,-k,s*h),new Vector3(k,-k,s*h),new Vector3(k,k,s*h),new Vector3(-k,k,s*h));
            for(int q=-1;q<=1;q+=2)
            {
                Face(new Vector3(s*h,-k,q*k),new Vector3(s*k,-k,q*h),new Vector3(s*k,k,q*h),new Vector3(s*h,k,q*k));
                Face(new Vector3(-k,s*h,q*k),new Vector3(-k,s*k,q*h),new Vector3(k,s*k,q*h),new Vector3(k,s*h,q*k));
                Face(new Vector3(s*h,q*k,-k),new Vector3(s*k,q*h,-k),new Vector3(s*k,q*h,k),new Vector3(s*h,q*k,k));
                for(int r=-1;r<=1;r+=2)Face(new Vector3(s*h,q*k,r*k),new Vector3(s*k,q*h,r*k),new Vector3(s*k,q*k,r*h));
            }
        }
        return StoreMesh("BevelledAshlar",v,t,uv);
    }

    private static Mesh BranchMesh()
    {
        var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();
        for(int row=0;row<4;row++)for(int i=0;i<=12;i++)
        {float a=i*Mathf.PI/6,r=Mathf.Lerp(.54f,.26f,row/3f)*(1+Mathf.Sin(i*3.1f)*.08f);v.Add(new Vector3(Mathf.Cos(a)*r,row/3f-.5f,Mathf.Sin(a)*r));uv.Add(new Vector2(i/12f,row/3f*3));}
        for(int row=0;row<3;row++)for(int i=0;i<12;i++){int a=row*13+i;t.AddRange(new[]{a,a+13,a+1,a+1,a+13,a+14});}
        return StoreMesh("RidgedOakBranch",v,t,uv);
    }

    private static Mesh LeafMesh()
    {
        var v=new List<Vector3>{new Vector3(0,-.5f,0),new Vector3(-.28f,-.20f,.02f),new Vector3(-.34f,.10f,.06f),new Vector3(-.18f,.34f,.03f),new Vector3(0,.5f,0),new Vector3(.18f,.34f,.03f),new Vector3(.34f,.10f,.06f),new Vector3(.28f,-.20f,.02f),new Vector3(0,.05f,-.05f)};
        var t=new List<int>();for(int i=0;i<8;i++)t.AddRange(new[]{8,i,(i+1)%8});
        var uv=new List<Vector2>();foreach(var p in v)uv.Add(new Vector2(p.x+.5f,p.y+.5f));return StoreMesh("CuppedOakLeaf",v,t,uv);
    }

    private static Mesh TemporaryMesh(List<Vector3> v,List<int> t,List<Vector2> uv)
    {var mesh=new Mesh();mesh.SetVertices(v);mesh.SetTriangles(t,0);mesh.SetUVs(0,uv);mesh.RecalculateNormals();mesh.RecalculateBounds();return mesh;}
    private static Mesh StoreMesh(string name,List<Vector3> v,List<int> t,List<Vector2> uv)
    {
        string path=Folder+name+".asset";var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(path);
        if(mesh==null){mesh=new Mesh{name=name};AssetDatabase.CreateAsset(mesh,path);}else mesh.Clear();
        mesh.SetVertices(v);mesh.SetTriangles(t,0);mesh.SetUVs(0,uv);mesh.RecalculateNormals();mesh.RecalculateBounds();EditorUtility.SetDirty(mesh);return mesh;
    }
}
