#if UNITY_EDITOR
using System.Collections;
using C02;
using NUnit.Framework;
using UnityEditor;
using UnityEngine;
using UnityEngine.TestTools;

namespace C02.Tests
{
    public sealed class C19ActorMotionTests
    {
        [UnityTest]
        [Category("PlayFeedbackRevision")]
        public IEnumerator RealModelAnimatesPartsWithoutMovingAuthorityOrRuntimeAttachments()
        {
            foreach (string model in new[] { "ObservedPlayer", "MonsterType0", "MonsterType1", "MonsterType2" })
            {
                var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/C02/Art/C04/" + model + ".prefab");
                Assert.That(prefab, Is.Not.Null, model);
                var actor = Object.Instantiate(prefab);
                try
                {
                    var hp = new GameObject("Runtime HP"); hp.transform.SetParent(actor.transform, false);
                    var range = new GameObject("Runtime range", typeof(LineRenderer)); range.transform.SetParent(actor.transform, false);
                    var motion = actor.AddComponent<C19ActorMotion>(); motion.Configure(); motion.Configure();
                    var visual = actor.transform.Find("GuardVisual") ?? actor.transform.Find("MotionVisual");
                    Assert.That(visual, Is.Not.Null);
                    var arm = visual.Find("Shoulder1");
                    Assert.That(arm, Is.Not.Null, "The real attacking arm must be attached to its shoulder pivot: " + model);
                    Assert.That(arm.childCount, Is.GreaterThan(0));
                    var leg = visual.Find("Hip1");
                    Assert.That(leg, Is.Not.Null, model);
                    Assert.That(leg.childCount, Is.GreaterThan(0), model);
                    Vector3 rest = visual.localPosition;
                    yield return null;
                    actor.transform.position += new Vector3(.1f, 0, .1f);
                    yield return new WaitForEndOfFrame();
                    Assert.That(Quaternion.Angle(leg.localRotation, Quaternion.identity), Is.GreaterThan(.01f), model);
                    Vector3 authority = actor.transform.position;
                    Quaternion direction = actor.transform.rotation;
                    motion.PlayAttack(0, true);
                    yield return new WaitForSecondsRealtime(.08f);
                    Assert.That(Quaternion.Angle(arm.localRotation, Quaternion.identity), Is.GreaterThan(1));
                    motion.PlayHit();
                    yield return new WaitForSecondsRealtime(.08f);
                    Assert.That(Vector3.Distance(visual.localPosition, rest), Is.GreaterThan(.001f));
                    Assert.That(actor.transform.position, Is.EqualTo(authority));
                    Assert.That(actor.transform.rotation, Is.EqualTo(direction));
                    Assert.That(hp.transform.parent, Is.EqualTo(actor.transform));
                    Assert.That(range.transform.parent, Is.EqualTo(actor.transform));
                    motion.ResetPose();
                    Assert.That(visual.localPosition, Is.EqualTo(rest));
                    Assert.That(Quaternion.Angle(arm.localRotation, Quaternion.identity), Is.LessThan(.001f));
                    if (model == "ObservedPlayer")
                    {
                        motion.SetMovementIntent(Vector2.right);
                        yield return new WaitForSecondsRealtime(.15f);
                        Assert.That(Vector3.Dot(visual.TransformDirection(Vector3.back), Vector3.right), Is.GreaterThan(.5f));
                        actor.transform.position -= Vector3.right * .3f;
                        yield return new WaitForEndOfFrame();
                        Assert.That(Vector3.Dot(visual.TransformDirection(Vector3.back), Vector3.right), Is.GreaterThan(.5f),
                            "A backward position correction must not turn the local actor against held input.");
                        motion.SetMovementIntent(Vector2.zero);
                        actor.transform.position -= Vector3.right * .1f;
                        yield return null;
                        yield return new WaitForEndOfFrame();
                        Assert.That(Vector3.Dot(visual.TransformDirection(Vector3.back), Vector3.right), Is.GreaterThan(.5f),
                            "Reconciliation after key release preserves the last facing.");
                        Assert.That(Quaternion.Angle(leg.localRotation, Quaternion.identity), Is.LessThan(.001f),
                            "Correction displacement does not animate idle local legs.");
                    }
                }
                finally { Object.DestroyImmediate(actor); }
            }
        }
    }
}
#endif
