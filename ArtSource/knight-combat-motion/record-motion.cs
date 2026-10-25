using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using UnityEngine;
using UnityEditor;
using C02;
using ClientCore.PacketHelper;
public static class KnightMotionRecording {
 const BindingFlags Flags=BindingFlags.Instance|BindingFlags.NonPublic;
 static object Call(object o,string n,params object[] args)=>o.GetType().GetMethod(n,Flags).Invoke(o,args);
 static void Set(object o,string n,object value)=>o.GetType().GetField(n,Flags).SetValue(o,value);
 static GameObject GetRoot(object o,string n)=>(GameObject)o.GetType().GetField(n,Flags).GetValue(o);
 public static async Task<string> Main() {
  const string version="after";
  if(!Application.isPlaying)throw new InvalidOperationException("Require Play for real elapsed motion samples.");
  if(version!="before"&&version!="after"&&version!="after-strike-warm")throw new ArgumentException("Use before or after.");
  string output=Path.Combine(Application.dataPath,"../Builds/Client-Resume/CombatMotion",version);
  if(Directory.Exists(output))throw new IOException("Preserve existing capture directory: "+output);
  Directory.CreateDirectory(output);
  var preview=new PreviewRenderUtility();var createdMaterials=new List<Material>();
  GameObject controls=null;Texture2D image=null;
  var log=new System.Text.StringBuilder("Authored PlayMode preview; staged phase calls, no network/input.\nslot,frame,elapsed,engineFrame\n");
  int firstFrame=Time.frameCount,frames=0;
  try {
   var actor=UnityEngine.Object.Instantiate(AssetDatabase.LoadAssetAtPath<GameObject>("Assets/C02/Art/C04/ObservedPlayer.prefab"));
   preview.AddSingleGO(actor);actor.transform.SetPositionAndRotation(Vector3.zero,Quaternion.identity);
   var motion=actor.AddComponent<C19ActorMotion>();motion.Configure();motion.enabled=false;
   controls=new GameObject("Temporary capture controls");controls.SetActive(false);
   var combat=controls.AddComponent<C05CombatScreen>();
   var content=AssetDatabase.LoadAssetAtPath<C14ServerContent>("Assets/C02/Content/C14ServerContent.asset");
   Set(combat,"serverContent",content);Set(combat,"skillEffectMaterial",AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/KnightRefinement/SteelSkillGlow.mat"));
   var floor=GameObject.CreatePrimitive(PrimitiveType.Plane);preview.AddSingleGO(floor);
   floor.transform.localScale=Vector3.one*3;floor.transform.position=new Vector3(0,-.02f,0);
   var ground=new Material(Shader.Find("Universal Render Pipeline/Lit"));ground.color=new Color(.16f,.18f,.15f);ground.SetFloat("_Smoothness",.12f);createdMaterials.Add(ground);floor.GetComponent<Renderer>().sharedMaterial=ground;
   var probeGo=new GameObject("Temporary capture reflection");preview.AddSingleGO(probeGo);var probe=probeGo.AddComponent<ReflectionProbe>();probe.mode=UnityEngine.Rendering.ReflectionProbeMode.Custom;probe.customBakedTexture=AssetDatabase.LoadAssetAtPath<Cubemap>("Assets/C02/Art/CitadelEnvironment/CitadelReflection.asset");probe.size=Vector3.one*50;probe.intensity=1.1f;
   preview.camera.orthographic=true;preview.camera.orthographicSize=3.2f;preview.camera.nearClipPlane=.01f;preview.camera.farClipPlane=80;
   preview.camera.clearFlags=CameraClearFlags.Color;preview.camera.backgroundColor=new Color(.115f,.13f,.145f);
   preview.camera.transform.position=new Vector3(6,7,-8);preview.camera.transform.LookAt(new Vector3(0,1.1f,0));
   preview.lights[0].intensity=1.8f;preview.lights[0].color=new Color(1,.95f,.87f);preview.lights[0].transform.rotation=Quaternion.Euler(38,-35,0);
   preview.lights[1].intensity=1.2f;preview.lights[1].color=new Color(.8f,.88f,1);preview.lights[1].transform.rotation=Quaternion.Euler(30,140,0);preview.ambientColor=new Color(.38f,.4f,.44f);
   preview.BeginStaticPreview(new Rect(0,0,720,540));preview.Render(true);image=preview.EndStaticPreview();image.EncodeToPNG();UnityEngine.Object.DestroyImmediate(image);image=null;
   await Task.Delay(50);
   for(byte slot=0;slot<3;slot++) {
    motion.ResetPose();Call(combat,"HideChargeTelegraph");Call(combat,"HideReleaseEffect");
    var skill=slot==1?content.ring:Array.Find(content.otherSkills,x=>x.skillID==slot);
    float charge=skill.phases[1].waitTicks*.05f;
    var phase=new ActionResultField{casterType=0,casterId=73,dir=1,x=0,y=0,skillSlot=slot,skillId=slot,skillPhase=0};
    motion.PlayAttack(1,false,slot,charge);Call(combat,"ShowChargeTelegraph",phase);
    var root=GetRoot(combat,"chargeTelegraphRoot");if(root!=null)preview.AddSingleGO(root);
    float started=Time.unscaledTime,next=started;bool released=false;int frame=0;
    string dir=Path.Combine(output,"slot-"+slot);Directory.CreateDirectory(dir);
    while(Time.unscaledTime-started<charge+1f) {
     if(!Application.isPlaying)throw new InvalidOperationException("Play interrupted.");
     float elapsed=Time.unscaledTime-started;
     if(!released&&elapsed>=charge) {
      released=true;phase.skillPhase=1;motion.PlayAttack(1,true,slot);Call(combat,"HideChargeTelegraph");Call(combat,"ShowReleaseEffect",phase);
      root=GetRoot(combat,"releaseEffectRoot");if(root!=null)preview.AddSingleGO(root);
     }
     Call(motion,"LateUpdate");Call(combat,"UpdateChargeTelegraph");Call(combat,"UpdateReleaseEffect");
     if(Time.unscaledTime>=next) {
      preview.BeginStaticPreview(new Rect(0,0,720,540));preview.Render(true);image=preview.EndStaticPreview();
      File.WriteAllBytes(Path.Combine(dir,"frame-"+frame.ToString("D3")+".png"),image.EncodeToPNG());UnityEngine.Object.DestroyImmediate(image);image=null;
      log.AppendLine(slot+","+frame+","+elapsed.ToString("F4",System.Globalization.CultureInfo.InvariantCulture)+","+Time.frameCount);frame++;frames++;next=Time.unscaledTime+.05f;
     }
     await Task.Delay(10);
    }
   }
   if(Time.frameCount<=firstFrame)throw new InvalidOperationException("No engine frame progress.");
   Call(combat,"DestroyEffectObjects");
   File.WriteAllText(Path.Combine(output,"capture.txt"),log.ToString()+"engineFrames="+(Time.frameCount-firstFrame)+"; images="+frames+"\n");
   return "Saved "+frames+" authored frames; engine advanced "+(Time.frameCount-firstFrame)+" frames. Source assets unchanged.";
  } finally {
   if(image!=null)UnityEngine.Object.DestroyImmediate(image);
   if(controls!=null){var c=controls.GetComponent<C05CombatScreen>();if(c!=null)Call(c,"DestroyEffectObjects");UnityEngine.Object.DestroyImmediate(controls);}
   preview.Cleanup();foreach(var m in createdMaterials)if(m!=null)UnityEngine.Object.DestroyImmediate(m);
  }
 }
}
