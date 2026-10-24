#if UNITY_EDITOR
using System.Collections;
using System.Reflection;
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
        public IEnumerator KnightSkillSlotsHaveDistinctChargeCutAndRecoveryPosesWithoutMovingAuthority()
        {
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/C02/Art/C04/ObservedPlayer.prefab");
            Assert.That(prefab, Is.Not.Null);
            var actor = Object.Instantiate(prefab);
            try
            {
                yield return null;
                actor.transform.SetPositionAndRotation(new Vector3(7f, 0f, 11f), Quaternion.Euler(0f, 37f, 0f));
                var motion = actor.AddComponent<C19ActorMotion>();
                motion.Configure();
                motion.ResetPose();
                var visual = actor.transform.Find("V01Hero") ?? actor.transform.Find("GuardVisual");
                Assert.That(visual, Is.Not.Null);
                var upperBody = visual.Find("UpperBody");
                var swordArm = upperBody != null ? upperBody.Find("Shoulder1") : visual.Find("Shoulder1");
                var supportArm = upperBody != null ? upperBody.Find("Shoulder-1") : visual.Find("Shoulder-1");
                var leftLeg = visual.Find("Hip-1");
                var rightLeg = visual.Find("Hip1");
                Assert.That(swordArm, Is.Not.Null);
                Assert.That(supportArm, Is.Not.Null);
                Assert.That(leftLeg, Is.Not.Null);
                Assert.That(rightLeg, Is.Not.Null);
                Assert.That(upperBody, Is.Not.Null);
                Assert.That(swordArm.parent, Is.EqualTo(upperBody));
                Assert.That(supportArm.parent, Is.EqualTo(upperBody));
                Assert.That(leftLeg.parent, Is.EqualTo(visual));
                Assert.That(rightLeg.parent, Is.EqualTo(visual));

                Vector3 authorityPosition = actor.transform.position;
                Quaternion authorityRotation = actor.transform.rotation;
                var chargePoses = new Quaternion[3];
                var releasePoses = new Quaternion[3];
                var supportPoses = new Quaternion[3];
                var torsoPoses = new Quaternion[3];
                var leftLegPoses = new Quaternion[3];
                var rightLegPoses = new Quaternion[3];
                var durations = new[] { .48f, .48f, .58f };

                for (byte slot = 0; slot < 3; slot++)
                {
                    motion.PlayAttack(0, false, slot);
                    yield return new WaitForSecondsRealtime(.3f);
                    chargePoses[slot] = swordArm.localRotation;
                    supportPoses[slot] = supportArm.localRotation;
                    torsoPoses[slot] = upperBody.localRotation;
                    AssertAuthority(actor, authorityPosition, authorityRotation);

                    float releaseStarted = Time.unscaledTime;
                    Vector3 ringFacing = visual.TransformDirection(Vector3.back);
                    Quaternion previousRootRotation = visual.localRotation;
                    float ringRotationDegrees = 0f;
                    motion.PlayAttack(0, true, slot);
                    yield return new WaitForEndOfFrame();
                    if (slot == 1)
                    {
                        ringRotationDegrees += Quaternion.Angle(previousRootRotation, visual.localRotation);
                        previousRootRotation = visual.localRotation;
                    }
                    Quaternion firstPhaseFrame = swordArm.localRotation;
                    Assert.That(Quaternion.Angle(firstPhaseFrame, chargePoses[slot]), Is.LessThan(5f),
                        "The knight starts the release from its displayed charging pose.");
                    yield return new WaitForSecondsRealtime(.12f);
                    releasePoses[slot] = swordArm.localRotation;
                    leftLegPoses[slot] = leftLeg.localRotation;
                    rightLegPoses[slot] = rightLeg.localRotation;
                    Assert.That(Quaternion.Angle(releasePoses[slot], Quaternion.identity), Is.GreaterThan(5f),
                        "Each server-confirmed release has a visible weapon pose.");
                    Assert.That(Quaternion.Angle(firstPhaseFrame, releasePoses[slot]), Is.GreaterThan(5f),
                        "The release progresses into its attack pose after the charging transition.");
                    if (slot == 1)
                    {
                        ringRotationDegrees += Quaternion.Angle(previousRootRotation, visual.localRotation);
                        previousRootRotation = visual.localRotation;
                    }
                    AssertAuthority(actor, authorityPosition, authorityRotation);

                    // A frame can straddle the deadline by different amounts. Check the
                    // observable recovered pose after the clip, not a fixed angular delta
                    // across an unspecified number of rendered frames.
                    while (Time.unscaledTime - releaseStarted < durations[slot] + .2f)
                    {
                        yield return null;
                        if (slot == 1)
                        {
                            ringRotationDegrees += Quaternion.Angle(previousRootRotation, visual.localRotation);
                            previousRootRotation = visual.localRotation;
                        }
                    }
                    yield return new WaitForEndOfFrame();
                    if (slot == 1)
                        ringRotationDegrees += Quaternion.Angle(previousRootRotation, visual.localRotation);
                    Assert.That(Quaternion.Angle(swordArm.localRotation, Quaternion.identity), Is.LessThan(3f),
                        "The weapon settles at its resting pose after each skill.");
                    if (slot == 1)
                    {
                        Assert.That(ringRotationDegrees, Is.GreaterThan(300f),
                            "The released ring skill completes a tracked full-body turn across rendered frames.");
                        Assert.That(Vector3.Angle(ringFacing, visual.TransformDirection(Vector3.back)), Is.LessThan(1f),
                            "The ring sweep completes a full turn and returns to its accepted facing.");
                        motion.SetMovementIntent(Vector2.right);
                        yield return new WaitForSecondsRealtime(.2f);
                        yield return new WaitForEndOfFrame();
                        Assert.That(Vector3.Dot(visual.TransformDirection(Vector3.back), Vector3.right), Is.GreaterThan(.95f),
                            "After the completion frame, normal facing smoothing follows the next movement direction.");
                        motion.SetMovementIntent(Vector2.zero);
                    }
                    AssertAuthority(actor, authorityPosition, authorityRotation);
                }
                Assert.That(Quaternion.Angle(chargePoses[0], chargePoses[1]), Is.GreaterThan(5f));
                Assert.That(Quaternion.Angle(chargePoses[1], chargePoses[2]), Is.GreaterThan(5f));
                Assert.That(Quaternion.Angle(supportPoses[0], supportPoses[1]), Is.GreaterThan(5f),
                    "The supporting arm takes a distinct counterbalance pose for the ring sweep.");
                Assert.That(Quaternion.Angle(torsoPoses[0], torsoPoses[2]), Is.GreaterThan(1f),
                    "The torso joins the strike and overhead preparation.");
                Assert.That(Quaternion.Angle(leftLegPoses[2], rightLegPoses[2]), Is.GreaterThan(1f),
                    "The cleave stance shifts the legs rather than animating the sword arm alone.");
                Assert.That(Quaternion.Angle(releasePoses[0], releasePoses[1]), Is.GreaterThan(10f));
                Assert.That(Quaternion.Angle(releasePoses[1], releasePoses[2]), Is.GreaterThan(10f));

                var transitionUntil = typeof(C19ActorMotion).GetField("phaseTransitionUntil", BindingFlags.Instance | BindingFlags.NonPublic);
                motion.ResetPose();
                motion.PlayAttack(0, true, 0);
                Assert.That((float)transitionUntil.GetValue(motion), Is.Zero,
                    "A first release without an observed charge has no phase blend.");
                motion.ResetPose();
                motion.PlayAttack(0, false, 0);
                yield return new WaitForSecondsRealtime(.56f);
                motion.PlayAttack(0, true, 0);
                Assert.That((float)transitionUntil.GetValue(motion), Is.Zero,
                    "An expired charge cannot blend into a later release.");
                motion.ResetPose();
                motion.PlayAttack(0, false, 0);
                yield return new WaitForSecondsRealtime(.08f);
                motion.PlayAttack(0, true, 1);
                Assert.That((float)transitionUntil.GetValue(motion), Is.Zero,
                    "A release from another skill slot cannot reuse the charge pose.");
            }
            finally { Object.DestroyImmediate(actor); }
        }

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
                    var upperBody = visual.Find("UpperBody");
                    var arm = upperBody != null ? upperBody.Find("Shoulder1") : visual.Find("Shoulder1");
                    Assert.That(arm, Is.Not.Null, "The real attacking arm must be attached to its shoulder pivot: " + model);
                    Assert.That(arm.childCount, Is.GreaterThan(0));
                    var leg = visual.Find("Hip1");
                    Assert.That(leg, Is.Not.Null, model);
                    Assert.That(leg.childCount, Is.GreaterThan(0), model);
                    Vector3 rest = visual.localPosition;
                    yield return null;
                    float walkStart = Time.realtimeSinceStartup;
                    Vector3 walkDirection = new Vector3(1f, 0f, 1f).normalized;
                    while (Time.realtimeSinceStartup - walkStart < .15f)
                    {
                        actor.transform.position += walkDirection * (2.5f * Time.unscaledDeltaTime);
                        yield return new WaitForEndOfFrame();
                    }
                    Assert.That(Quaternion.Angle(leg.localRotation, Quaternion.identity), Is.GreaterThan(.01f), model);
                    Assert.That(Quaternion.Angle(arm.localRotation, Quaternion.identity), Is.GreaterThan(.01f),
                        "Walking arm swing remains visible: " + model);
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

        private static void AssertAuthority(GameObject actor, Vector3 position, Quaternion rotation)
        {
            Assert.That(actor.transform.position, Is.EqualTo(position), "Presentation never moves the authoritative actor root.");
            Assert.That(actor.transform.rotation, Is.EqualTo(rotation), "Presentation never rotates the authoritative actor root.");
        }
    }
}
#endif
