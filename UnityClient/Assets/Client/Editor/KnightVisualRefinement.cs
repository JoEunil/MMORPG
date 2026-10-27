using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using Object = UnityEngine.Object;

namespace Mmorpg.Client.Editor
{
    // Original mesh/texture authoring for the existing unrigged knight. No imported resources.
    // Runtime motion still owns the original named parts and the network root is untouched.
    public static class KnightVisualRefinement
    {
        private const string Folder = "Assets/Client/Art/KnightRefinement/";
        private const string PlayerPrefab = "Assets/Client/Art/Actors/ObservedPlayer.prefab";

        [MenuItem("Tools/Client/Art/Refine Existing Knight Materials Only")]
        public static void RefreshMaterialReadability()
        {
            if(EditorApplication.isPlayingOrWillChangePlaymode)
                throw new InvalidOperationException("Knight material authoring requires Edit Mode.");
            foreach(var material in ConfigureMaterialReadability())AssetDatabase.SaveAssetIfDirty(material);
            Debug.Log("Existing KnightRefinement material tones and roughness refined; meshes, textures, motion pivots, scene objects and lighting unchanged.");
        }

        private static Material[] ConfigureMaterialReadability()
        {
            var surfaces=new[]
            {
                (name:"TemperedSteel",color:new Color(.74f,.76f,.77f),metallic:.55f,smoothness:.38f),
                (name:"PolishedEdge",color:new Color(.72f,.73f,.72f),metallic:.85f,smoothness:.56f),
                (name:"AntiqueBrass",color:new Color(.36f,.29f,.17f),metallic:.72f,smoothness:.32f),
                (name:"RecessSteel",color:new Color(.045f,.052f,.058f),metallic:.50f,smoothness:.21f),
                (name:"OxbloodCloth",color:new Color(.29f,.055f,.06f),metallic:0f,smoothness:.09f),
                (name:"DarkLeather",color:new Color(.09f,.065f,.045f),metallic:0f,smoothness:.16f)
            };
            var materials=new Material[surfaces.Length];
            for(int i=0;i<surfaces.Length;i++)
            {
                var material=AssetDatabase.LoadAssetAtPath<Material>(Folder+surfaces[i].name+".mat");
                if(material==null || material.shader==null || material.shader.name!="Universal Render Pipeline/Lit")
                    throw new InvalidOperationException("The existing URP Lit knight material is required: "+surfaces[i].name);
                materials[i]=material;
            }
            for(int i=0;i<surfaces.Length;i++)
            {
                var material=materials[i];
                material.color=surfaces[i].color;
                material.SetFloat("_Metallic",surfaces[i].metallic);
                material.SetFloat("_Smoothness",surfaces[i].smoothness);
                EditorUtility.SetDirty(material);
            }
            // Reduce glitter from the existing micro-normal; preserve the actual normal/albedo assets.
            materials[0].SetFloat("_BumpScale",.28f);
            return materials;
        }

        [MenuItem("Tools/Client/Art/Refine Existing Knights")]
        public static void Apply()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode)
                throw new InvalidOperationException("Knight art must be applied outside Play Mode.");
            Scene scene = SceneManager.GetActiveScene();
            if (scene.path != "Assets/Client/Scenes/World.unity")
                throw new InvalidOperationException("Open the existing C02 scene before applying knight art.");
            EnsureAssets();
            int count = 0;
            foreach (var root in scene.GetRootGameObjects())
                foreach (var part in root.GetComponentsInChildren<Transform>(true))
                    if (part != null && (part.name == "V01Hero" || part.name == "GuardVisual") && part.Find("Breastplate") != null)
                    { Refine(part); count++; }
            foreach(var root in scene.GetRootGameObjects())
                if(root.name=="V01LoginDiorama") PreviewLighting(root.transform);
            var prefab = PrefabUtility.LoadPrefabContents(PlayerPrefab);
            try
            {
                var visual = prefab.transform.Find("GuardVisual");
                if (visual == null) throw new InvalidOperationException("ObservedPlayer has no GuardVisual.");
                Refine(visual);
                PrefabUtility.SaveAsPrefabAsset(prefab, PlayerPrefab);
            }
            finally { PrefabUtility.UnloadPrefabContents(prefab); }
            AssetDatabase.SaveAssets();
            EditorSceneManager.MarkSceneDirty(scene);
            if (!EditorSceneManager.SaveScene(scene)) throw new IOException("Could not save the refined knight scene.");
            Debug.Log("Knight refinement applied to " + count + " scene visuals and ObservedPlayer; original motion pivots retained.");
        }

        // Accepts the existing V01Hero / GuardVisual, before runtime motion creates joints.
        public static void Refine(Transform visual)
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode)
                throw new InvalidOperationException("Refinement is an edit-time operation.");
            if (visual == null || visual.Find("Breastplate") == null)
                throw new ArgumentException("Expected the existing knight visual with direct named parts.");
            EnsureAssets();
            var steel = AssetDatabase.LoadAssetAtPath<Material>(Folder + "TemperedSteel.mat");
            var edge = AssetDatabase.LoadAssetAtPath<Material>(Folder + "PolishedEdge.mat");
            var gold = AssetDatabase.LoadAssetAtPath<Material>(Folder + "AntiqueBrass.mat");
            var dark = AssetDatabase.LoadAssetAtPath<Material>(Folder + "RecessSteel.mat");
            var cloth = AssetDatabase.LoadAssetAtPath<Material>(Folder + "OxbloodCloth.mat");
            var leather = AssetDatabase.LoadAssetAtPath<Material>(Folder + "DarkLeather.mat");
            foreach (var renderer in visual.GetComponentsInChildren<MeshRenderer>(true))
            {
                string n = renderer.sharedMaterial != null ? renderer.sharedMaterial.name : "";
                if (n == "GuardSteel" || n == "TemperedSteel") renderer.sharedMaterial = steel;
                else if (n == "WornSteelEdge" || n == "PolishedEdge") renderer.sharedMaterial = edge;
                else if (n == "AgedBronze" || n == "AntiqueBrass") renderer.sharedMaterial = gold;
                else if (n == "FadedCrimson" || n == "OxbloodCloth") renderer.sharedMaterial = cloth;
                else if (n == "OiledLeather" || n == "DarkLeather") renderer.sharedMaterial = leather;
                else if (n == "VisorRecess" || n == "RecessSteel") renderer.sharedMaterial = dark;
            }

            // Each replacement remains on the original object; added details follow that object's motion.
            Replace(visual, "Breastplate", Shell("Breastplate", new[] {1.13f,1.17f,1.23f,1.40f,1.55f,1.60f,1.63f},
                new[] {.175f,.188f,.187f,.248f,.266f,.244f,.207f},
                new[] {.134f,.153f,.163f,.185f,.170f,.142f,.120f}, .025f), steel);
            Transform breast = visual.Find("Breastplate");
            Stroke("KnightChestRidge", breast, new[] {new Vector3(0,1.18f,-.181f),new Vector3(0,1.41f,-.211f),new Vector3(0,1.58f,-.181f)}, .009f, edge);
            for (int s = -1; s <= 1; s += 2)
            {
                Stroke("KnightChestFlute" + s, breast, new[] {new Vector3(s*.058f,1.23f,-.166f),new Vector3(s*.105f,1.44f,-.171f),new Vector3(s*.178f,1.565f,-.126f)}, .004f, edge);
                Stroke("KnightCollarTrim" + s, breast, new[] {new Vector3(0,1.59f,-.176f),new Vector3(s*.11f,1.59f,-.143f),new Vector3(s*.188f,1.615f,-.070f)}, .007f, gold);
            }
            // Small embossed heraldry reads as metal relief, rather than a separate bright cube.
            Extrusion("KnightChestHeraldry", breast, new[] {new Vector2(0,1.49f),new Vector2(.018f,1.445f),new Vector2(.060f,1.423f),new Vector2(.018f,1.405f),new Vector2(0,1.36f),new Vector2(-.018f,1.405f),new Vector2(-.060f,1.423f),new Vector2(-.018f,1.445f)}, -.207f, .007f, gold);

            Replace(visual, "HelmetShell", Shell("Sallet", new[] {1.745f,1.78f,1.86f,1.95f,2.035f,2.075f},
                new[] {.105f,.127f,.129f,.119f,.070f,.005f}, new[] {.123f,.145f,.149f,.134f,.078f,.014f}, .014f), steel);
            Replace(visual, "Gorget", Shell("ClosedGorget", new[] {1.61f,1.65f,1.71f,1.775f},
                new[] {.198f,.147f,.108f,.105f}, new[] {.133f,.114f,.105f,.105f}, .003f), steel);
            Transform helmet = visual.Find("HelmetShell");
            Stroke("KnightHelmetCrest", helmet, new[] {new Vector3(0,1.955f,-.14f),new Vector3(0,2.06f,-.066f),new Vector3(0,2.09f,.014f),new Vector3(0,2.04f,.08f),new Vector3(0,1.91f,.145f)}, .008f, edge);
            Transform visor = visual.Find("Visor");
            Stroke("KnightBrow", visor, new[] {new Vector3(-.123f,1.93f,-.10f),new Vector3(-.075f,1.934f,-.157f),new Vector3(0,1.925f,-.18f),new Vector3(.075f,1.934f,-.157f),new Vector3(.123f,1.93f,-.10f)}, .009f, edge);
            Stroke("KnightBevor", visor, new[] {new Vector3(-.105f,1.80f,-.10f),new Vector3(0,1.775f,-.163f),new Vector3(.105f,1.80f,-.10f)}, .008f, gold);

            for(int i=0;i<3;i++)
                Replace(visual,"FauldPlate"+i,Shell("ClosedFauld"+i,
                    new[]{1.105f-i*.065f,1.12f-i*.065f,1.174f-i*.065f,1.185f-i*.065f},
                    new[]{.225f+i*.011f,.229f+i*.011f,.210f+i*.011f,.205f+i*.011f},
                    new[]{.162f,.167f,.159f,.155f},.006f),steel);

            for (int s = -1; s <= 1; s += 2)
            {
                // Closed overlapping shells eliminate the open, saw-toothed hems of the old tubes.
                // Their local axis still follows the original hip/knee/ankle pivots.
                Replace(visual,"Trouser"+s,Shell("ClosedPaddedLeg",new[]{-.018f,.035f,.28f,.47f},
                    new[]{.097f,.102f,.085f,.073f},new[]{.086f,.090f,.074f,.065f},0f),cloth);
                Replace(visual,"Cuissard"+s,Shell("ClosedCuissard",new[]{0f,.02f,.15f,.29f,.325f},
                    new[]{.091f,.101f,.096f,.084f,.077f},new[]{.079f,.087f,.083f,.075f,.071f},.006f),steel);
                Replace(visual,"KneeCop"+s,Shell("ClosedKneeCop",new[]{-.012f,.012f,.065f,.11f},
                    new[]{.076f,.090f,.086f,.070f},new[]{.070f,.081f,.078f,.064f},.009f),edge);
                Replace(visual,"Greave"+s,Shell("ClosedGreave",new[]{-.015f,.035f,.20f,.35f,.41f},
                    new[]{.076f,.087f,.076f,.059f,.049f},new[]{.070f,.080f,.066f,.051f,.045f},.009f),steel);
                Replace(visual,"Sabatons"+s,Shell("ArticulatedSabaton",new[]{.026f,.045f,.09f,.13f,.19f,.23f},
                    new[]{.073f,.076f,.074f,.067f,.052f,.047f},new[]{.154f,.160f,.157f,.131f,.076f,.059f},0f),steel);
                Replace(visual,"Boot"+s,Shell("BootSole",new[]{.005f,.018f,.04f},
                    new[]{.075f,.077f,.073f},new[]{.156f,.163f,.157f},0f),leather);

                // Taper the armor along the existing limb-local axes; leave motion pivots intact.
                // The flared elbow/cuff overlap their neighboring shells without balloon-like joints.
                Replace(visual,"UpperArmPlate"+s,Shell("ClosedRerebrace",new[]{-.006f,.018f,.075f,.150f,.180f},
                    new[]{.067f,.073f,.070f,.061f,.058f},new[]{.058f,.065f,.062f,.054f,.051f},.003f),steel);
                Replace(visual,"ElbowCop"+s,Shell("ClosedCouter",new[]{-.010f,.008f,.033f,.062f,.082f},
                    new[]{.060f,.074f,.078f,.070f,.060f},new[]{.056f,.068f,.073f,.065f,.055f},.004f),edge);
                Replace(visual,"Vambrace"+s,Shell("ClosedVambrace",new[]{-.008f,.016f,.070f,.168f,.221f},
                    new[]{.060f,.066f,.062f,.048f,.043f},new[]{.054f,.060f,.057f,.044f,.040f},.005f),steel);
                Replace(visual,"GauntletCuff"+s,Shell("ClosedGauntletCuff",new[]{-.008f,.006f,.031f,.058f},
                    new[]{.051f,.056f,.049f,.043f},new[]{.047f,.052f,.045f,.039f},.002f),edge);

                for (int layer = 0; layer < 3; layer++)
                {
                    string name = "ShoulderLame" + s + layer;
                    Transform plate = visual.Find(name);
                    float x = s*(.275f + layer*.022f), y = 1.62f-layer*.045f;
                    Replace(visual, name, Shoulder(name, new Vector3(x,y,0), .137f-layer*.012f, .138f-layer*.009f, .084f), steel);
                    var curve = new List<Vector3>();
                    for (int i=0;i<=32;i++) { float a=Mathf.PI*2*i/32; curve.Add(new Vector3(x+Mathf.Cos(a)*(.136f-layer*.012f),y-.011f,Mathf.Sin(a)*(.137f-layer*.009f))); }
                    Stroke("KnightPauldronEdge", plate, curve.ToArray(), .006f, edge);
                }
                Transform vambrace=visual.Find("Vambrace"+s);
                Stroke("KnightBracerRidge",vambrace,new[]{new Vector3(s*.38f,1.285f,-.089f),new Vector3(s*.368f,1.12f,-.10f)},.006f,edge);
                Transform knee=visual.Find("KneeCop"+s);
                Extrusion("KnightKneeWing",knee,new[]{new Vector2(s*.14f,.64f),new Vector2(s*.224f,.59f),new Vector2(s*.192f,.54f),new Vector2(s*.14f,.56f)},-.075f,.012f,steel);
                Transform greave=visual.Find("Greave"+s);
                Stroke("KnightShinRidge",greave,new[]{new Vector3(s*.143f,.53f,-.084f),new Vector3(s*.154f,.22f,-.052f)},.008f,edge);
                Transform shoe=visual.Find("Sabatons"+s);
                for(int i=0;i<3;i++) Stroke("KnightFootLame"+i,shoe,new[]{new Vector3(s*.155f-.054f,.104f,-.11f-i*.032f),new Vector3(s*.155f,.12f,-.12f-i*.032f),new Vector3(s*.155f+.054f,.104f,-.11f-i*.032f)},.004f,edge);
            }

            Transform sword=visual.Find("SwordBlade");
            // The original blade points down in its rest pose; retain the pivot used by sword motion.
            var blade = ExtrudedBlade();
            sword.GetComponent<MeshFilter>().sharedMesh=blade;
            sword.GetComponent<MeshRenderer>().sharedMaterial=edge;
            Stroke("KnightBladeFuller",sword,new[]{new Vector3(0,.27f,-.009f),new Vector3(0,.80f,-.009f)},.003f,dark,false);
            Transform guard=visual.Find("SwordGuard");
            guard.GetComponent<MeshRenderer>().enabled=false;
            // The replacement guard follows the original sword blade, in blade-local coordinates.
            Extrusion("KnightSweptGuard",sword,new[]{new Vector2(-.118f,.905f),new Vector2(-.095f,.935f),new Vector2(-.039f,.932f),new Vector2(0,.916f),new Vector2(.039f,.932f),new Vector2(.095f,.935f),new Vector2(.118f,.905f),new Vector2(.078f,.912f),new Vector2(0,.900f),new Vector2(-.078f,.912f)},-.020f,.035f,gold,false);

            Transform shield=visual.Find("ShieldForgedRim");
            Vector2[] outline={new Vector2(-.19f,.29f),new Vector2(.19f,.29f),new Vector2(.20f,.03f),new Vector2(.14f,-.18f),new Vector2(0,-.34f),new Vector2(-.14f,-.18f),new Vector2(-.20f,.03f)};
            var rim = new Vector3[outline.Length+1];
            for(int i=0;i<rim.Length;i++){Vector2 p=outline[i%outline.Length];rim[i]=new Vector3(-.435f+p.x,1.19f+p.y,-.186f+Mathf.Abs(p.x)*.16f);}
            Stroke("KnightShieldBeadedRim",shield,rim,.011f,edge);
            for(int i=0;i<outline.Length;i++)
            {
                Vector2 p=outline[i]*.88f;
                var rivet=MeshPart("KnightShieldRivet"+i,shield,Sphere("Rivet",.009f),gold);
                rivet.localPosition=new Vector3(-.435f+p.x,1.19f+p.y,-.207f+Mathf.Abs(p.x)*.16f);
            }
            Extrusion("KnightShieldCrest",shield,new[]{new Vector2(-.435f,1.40f),new Vector2(-.411f,1.29f),new Vector2(-.345f,1.26f),new Vector2(-.411f,1.23f),new Vector2(-.435f,1.10f),new Vector2(-.459f,1.23f),new Vector2(-.525f,1.26f),new Vector2(-.459f,1.29f)},-.229f,.013f,gold);
            foreach(string old in new[]{"ShieldInsignia","ShieldInsigniaBar"})
                if(visual.Find(old)!=null) visual.Find(old).GetComponent<MeshRenderer>().enabled=false;
            foreach(var filter in visual.GetComponentsInChildren<MeshFilter>(true))
            {
                var mesh=filter.sharedMesh;
                if(mesh!=null && mesh.uv.Length==mesh.vertexCount && mesh.tangents.Length!=mesh.vertexCount)
                {mesh.RecalculateTangents();EditorUtility.SetDirty(mesh);}
            }
        }

        private static void PreviewLighting(Transform diorama)
        {
            for(int i=0;i<2;i++)
            {
                string name=i==0?"KnightWarmRim":"KnightCoolFill";
                Transform existing=diorama.Find(name);
                var light=existing!=null?existing.GetComponent<Light>():new GameObject(name,typeof(Light)).GetComponent<Light>();
                if(light==null)throw new InvalidOperationException("Preview light name is occupied: "+name);
                light.transform.SetParent(diorama,false);
                light.transform.localPosition=i==0?new Vector3(-1.8f,4.1f,1.2f):new Vector3(2.3f,2.4f,-3f);
                light.type=LightType.Point;light.range=8f;light.intensity=i==0?3.5f:1.7f;
                light.color=i==0?new Color(1f,.72f,.42f):new Color(.52f,.70f,1f);
                light.shadows=LightShadows.None;
            }
        }

        private static void Replace(Transform visual,string name,Mesh mesh,Material material)
        {
            var part=visual.Find(name);
            if(part==null) throw new InvalidOperationException("Missing knight part: "+name);
            part.GetComponent<MeshFilter>().sharedMesh=mesh;
            part.GetComponent<MeshRenderer>().sharedMaterial=material;
        }

        private static Transform MeshPart(string name,Transform parent,Mesh mesh,Material material)
        {
            // Generated details keep their serialized object IDs when authoring is reapplied.
            // Obsolete details must be removed explicitly instead of rebuilding every child.
            var part=parent.Find(name);
            if(part==null)
            {
                part=new GameObject(name,typeof(MeshFilter),typeof(MeshRenderer)).transform;
                part.SetParent(parent,false);
            }
            var filter=part.GetComponent<MeshFilter>();
            var renderer=part.GetComponent<MeshRenderer>();
            if(filter==null || renderer==null)
                throw new InvalidOperationException("Knight detail name is occupied by an incompatible object: "+name);
            part.localPosition=Vector3.zero;
            part.localRotation=Quaternion.identity;
            part.localScale=Vector3.one;
            filter.sharedMesh=mesh;
            renderer.sharedMaterial=material;
            return part;
        }

        private static Mesh SaveMesh(string name,List<Vector3> vertices,List<int> triangles,List<Vector2> uv)
        {
            string path=Folder+name+".asset";
            var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(path);
            if(mesh==null){mesh=new Mesh{name=name};AssetDatabase.CreateAsset(mesh,path);}else mesh.Clear();
            mesh.SetVertices(vertices);mesh.SetTriangles(triangles,0);mesh.SetUVs(0,uv);mesh.RecalculateNormals();mesh.RecalculateTangents();mesh.RecalculateBounds();
            EditorUtility.SetDirty(mesh);return mesh;
        }

        private static Mesh Shell(string name,float[] heights,float[] widths,float[] depths,float keel)
        {
            const int sides=40;
            var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();
            for(int j=0;j<heights.Length;j++)for(int i=0;i<=sides;i++)
            {
                float a=Mathf.PI*2*i/sides;
                float ridge=Mathf.Pow(Mathf.Max(0,-Mathf.Sin(a)),12)*keel;
                v.Add(new Vector3(Mathf.Cos(a)*widths[j],heights[j],Mathf.Sin(a)*depths[j]-ridge));uv.Add(new Vector2(i/(float)sides,heights[j]*2));
            }
            for(int j=0;j<heights.Length-1;j++)for(int i=0;i<sides;i++)
            {int a=j*(sides+1)+i,b=a+sides+1;t.AddRange(new[]{a,b,a+1,a+1,b,b+1});}
            // Separate cap vertices keep rim normals crisp instead of smoothing across a missing end.
            for(int end=0;end<2;end++)
            {
                int row=end==0?0:heights.Length-1, center=v.Count;
                v.Add(new Vector3(0,heights[row],0));uv.Add(new Vector2(.5f,.5f));
                for(int i=0;i<sides;i++){v.Add(v[row*(sides+1)+i]);uv.Add(new Vector2(.5f,.5f));}
                for(int i=0;i<sides;i++)
                {
                    int a=center+1+i,b=center+1+(i+1)%sides;
                    if(end==0)t.AddRange(new[]{center,a,b});else t.AddRange(new[]{center,b,a});
                }
            }
            return SaveMesh(name,v,t,uv);
        }

        private static Mesh Shoulder(string name,Vector3 center,float width,float depth,float height)
        {
            var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();
            // A complete rounded cap joins front, crown and rear; the old open front strip
            // read as a bright rectangular shelf at the game's elevated camera angle.
            const int sides=32;
            float[] angles={0f,.35f,.70f,1.10f,1.45f,1.70f};
            for(int row=0;row<angles.Length;row++)for(int i=0;i<=sides;i++)
            {
                float a=Mathf.PI*2*i/sides;
                float radius=Mathf.Sin(angles[row]);
                v.Add(center+new Vector3(Mathf.Cos(a)*width*radius,Mathf.Cos(angles[row])*height,Mathf.Sin(a)*depth*radius));
                uv.Add(new Vector2(i/(float)sides,row/(float)(angles.Length-1)));
            }
            for(int row=0;row<angles.Length-1;row++)for(int i=0;i<sides;i++)
            {
                int a=row*(sides+1)+i,b=a+sides+1;
                t.AddRange(new[]{a,a+1,b,a+1,b+1,b});
            }
            return SaveMesh(name,v,t,uv);
        }

        private static void Stroke(string name,Transform parent,Vector3[] path,float radius,Material material,bool visualSpace=true)
        {
            var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();const int sides=6;
            for(int p=0;p<path.Length;p++)
            {
                Vector3 tangent=(path[Mathf.Min(p+1,path.Length-1)]-path[Mathf.Max(0,p-1)]).normalized;
                Vector3 right=Vector3.Cross(tangent,Vector3.forward).normalized;
                if(right.sqrMagnitude<.1f)right=Vector3.right;
                Vector3 up=Vector3.Cross(tangent,right).normalized;
                for(int i=0;i<sides;i++){float a=Mathf.PI*2*i/sides;v.Add(path[p]+(right*Mathf.Cos(a)+up*Mathf.Sin(a))*radius);uv.Add(new Vector2(i/(float)sides,p));}
            }
            for(int p=0;p<path.Length-1;p++)for(int i=0;i<sides;i++){int a=p*sides+i,b=p*sides+(i+1)%sides;t.AddRange(new[]{a,a+sides,b,b,a+sides,b+sides});}
            if(visualSpace) for(int i=0;i<v.Count;i++) v[i]=parent.InverseTransformPoint(parent.parent.TransformPoint(v[i]));
            MeshPart(name,parent,SaveMesh(name + "_" + parent.name,v,t,uv),material);
        }

        private static void Extrusion(string name,Transform parent,Vector2[] outline,float z,float depth,Material material,bool visualSpace=true)
        {
            var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();
            Vector2 center=Vector2.zero;foreach(var p in outline)center+=p;center/=outline.Length;
            v.Add(new Vector3(center.x,center.y,z-depth*.45f));uv.Add(new Vector2(.5f,.5f));
            foreach(var p in outline){v.Add(new Vector3(p.x,p.y,z));uv.Add(p);}
            foreach(var p in outline){v.Add(new Vector3(p.x,p.y,z+depth));uv.Add(p);}
            for(int i=0;i<outline.Length;i++)
            {int a=i+1,b=(i+1)%outline.Length+1,c=a+outline.Length,d=b+outline.Length;t.AddRange(new[]{0,a,b,a,c,b,b,c,d});}
            if(visualSpace) for(int i=0;i<v.Count;i++) v[i]=parent.InverseTransformPoint(parent.parent.TransformPoint(v[i]));
            MeshPart(name,parent,SaveMesh(name + "_" + parent.name,v,t,uv),material);
        }

        private static Mesh ExtrudedBlade()
        {
            var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();
            float[] heights={.115f,.19f,.84f,.893f};float[] widths={.0004f,.024f,.033f,.029f};
            for(int j=0;j<4;j++)
            {
                float w=widths[j],h=heights[j];
                foreach(var p in new[]{new Vector3(-w,h,0),new Vector3(-w*.62f,h,-.007f),new Vector3(0,h,-.010f),new Vector3(w*.62f,h,-.007f),new Vector3(w,h,0),new Vector3(0,h,.009f)})
                {v.Add(p);uv.Add(new Vector2((p.x/w+1)*.5f,h));}
            }
            for(int j=0;j<3;j++)for(int i=0;i<6;i++){int a=j*6+i,b=j*6+(i+1)%6;t.AddRange(new[]{a,a+6,b,b,a+6,b+6});}
            return SaveMesh("KnightBevelledBlade",v,t,uv);
        }

        private static Mesh Sphere(string name,float radius)
        {
            var v=new List<Vector3>();var t=new List<int>();var uv=new List<Vector2>();
            for(int y=0;y<=6;y++)for(int x=0;x<=8;x++){float a=x*Mathf.PI/4,b=y*Mathf.PI/6;v.Add(new Vector3(Mathf.Cos(a)*Mathf.Sin(b),Mathf.Cos(b),Mathf.Sin(a)*Mathf.Sin(b))*radius);uv.Add(new Vector2(x/8f,y/6f));}
            for(int y=0;y<6;y++)for(int x=0;x<8;x++){int a=y*9+x;t.AddRange(new[]{a,a+9,a+1,a+1,a+9,a+10});}
            return SaveMesh(name,v,t,uv);
        }

        private static void EnsureAssets()
        {
            if(!AssetDatabase.IsValidFolder(Folder.TrimEnd('/'))){Directory.CreateDirectory(Folder);AssetDatabase.Refresh();}
            Material steel=MaterialAsset("TemperedSteel",new Color(.43f,.49f,.55f),.82f,.47f);
            Material edge=MaterialAsset("PolishedEdge",new Color(.72f,.77f,.79f),.9f,.66f);
            MaterialAsset("AntiqueBrass",new Color(.47f,.32f,.13f),.79f,.43f);
            MaterialAsset("RecessSteel",new Color(.025f,.032f,.04f),.4f,.22f);
            MaterialAsset("OxbloodCloth",new Color(.22f,.025f,.032f),0,.18f);
            MaterialAsset("DarkLeather",new Color(.075f,.045f,.025f),0,.3f);
            string path=Folder+"BrushedSteel.asset";
            var texture=AssetDatabase.LoadAssetAtPath<Texture2D>(path);
            if(texture==null)
            {
                const int size=256;
                texture=new Texture2D(size,size,TextureFormat.RGBA32,true){name="BrushedSteel",wrapMode=TextureWrapMode.Repeat,filterMode=FilterMode.Trilinear,anisoLevel=4};
                var pixels=new Color[size*size];
                for(int y=0;y<size;y++)for(int x=0;x<size;x++)
                {
                    float grain=Mathf.PerlinNoise(x*.32f,y*.016f)*.11f+Mathf.PerlinNoise(x*.9f,y*.08f)*.055f;
                    float scratch=(x*73+y*19)%239==0?.09f:0;
                    float value=.82f+grain+scratch;pixels[y*size+x]=new Color(value,value,value);
                }
                texture.SetPixels(pixels);texture.Apply(true,false);AssetDatabase.CreateAsset(texture,path);
            }
            var albedo=AssetDatabase.LoadAssetAtPath<Texture2D>(Folder+"tempered-steel-albedo.png");
            if(albedo==null)throw new FileNotFoundException("Import the tempered-steel material source before refinement.");
            steel.mainTexture=albedo;steel.color=new Color(.74f,.80f,.87f);steel.mainTextureScale=new Vector2(1.4f,1.4f);
            steel.SetFloat("_Metallic",.76f);steel.SetFloat("_Smoothness",.48f);
            edge.mainTexture=texture;edge.color=new Color(.52f,.57f,.63f);edge.mainTextureScale=new Vector2(2,3);
            edge.SetFloat("_Metallic",.88f);edge.SetFloat("_Smoothness",.68f);
            string normalPath=Folder+"ForgedMicroNormal.asset";
            var normal=AssetDatabase.LoadAssetAtPath<Texture2D>(normalPath);
            if(normal==null)
            {
                normal=new Texture2D(128,128,TextureFormat.RGBA32,true,true){name="ForgedMicroNormal",wrapMode=TextureWrapMode.Repeat,filterMode=FilterMode.Trilinear};
                var normals=new Color[128*128];
                for(int y=0;y<128;y++)for(int x=0;x<128;x++)
                {
                    float dx=Mathf.PerlinNoise((x+1)*.34f,y*.06f)-Mathf.PerlinNoise((x-1)*.34f,y*.06f);
                    float dy=Mathf.PerlinNoise(x*.34f,(y+1)*.06f)-Mathf.PerlinNoise(x*.34f,(y-1)*.06f);
                    Vector3 n=new Vector3(-dx*.5f,-dy*.5f,1).normalized;
                    normals[y*128+x]=new Color(n.x*.5f+.5f,n.y*.5f+.5f,n.z*.5f+.5f,1);
                }
                normal.SetPixels(normals);normal.Apply(true,false);AssetDatabase.CreateAsset(normal,normalPath);
            }
            steel.SetTexture("_BumpMap",normal);steel.SetFloat("_BumpScale",.55f);steel.EnableKeyword("_NORMALMAP");
            ConfigureMaterialReadability();
            EditorUtility.SetDirty(steel);EditorUtility.SetDirty(edge);
        }

        private static Material MaterialAsset(string name,Color color,float metallic,float smoothness)
        {
            string path=Folder+name+".mat";var material=AssetDatabase.LoadAssetAtPath<Material>(path);
            if(material!=null)return material;
            Shader shader=Shader.Find("Universal Render Pipeline/Lit");
            if(shader==null)throw new InvalidOperationException("The existing URP Lit shader is required.");
            material=new Material(shader){name=name,color=color};material.SetFloat("_Metallic",metallic);material.SetFloat("_Smoothness",smoothness);
            material.SetFloat("_Cull",0);AssetDatabase.CreateAsset(material,path);return material;
        }
    }
}
