using System.Collections.Generic;
using UnityEngine;

namespace Mmorpg.Client
{
    // Presentation only: the network/world components continue to own this actor's transform.
    [DisallowMultipleComponent]
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C19ActorMotion")]
    public sealed class ActorMotion : MonoBehaviour
    {
        private const float HitDurationSeconds = .28f;
        private const float ParrySuccessDuration = .38f;
        private Transform visual;
        private Vector3 restPosition;
        private Quaternion restRotation;
        private readonly Transform[] arms = new Transform[2];
        private readonly Transform[] legs = new Transform[2];
        private Transform upperBody;
        private readonly Quaternion[] phaseStartArms = new Quaternion[2];
        private readonly Quaternion[] phaseStartLegs = new Quaternion[2];
        private Vector3 previousPosition;
        private Quaternion facing;
        private bool hasFacing;
        private bool useMovementIntent;
        private Vector2 movementIntent;
        private float height;
        private float stride;
        private float walking;
        private float attackStarted;
        private float attackUntil;
        private bool released;
        private byte attackSlot;
        private bool guardMotion;
        private bool spinPendingCompletion;
        private Quaternion phaseStartVisualRotation;
        private Quaternion phaseStartUpperBodyRotation;
        private float phaseTransitionStarted;
        private float phaseTransitionUntil;
        private float hitUntil;
        private float parryUntil;
        private float parryStarted;
        private float parrySuccessUntil;
        private bool parrySucceeded;
        private readonly List<MeshRenderer> shieldRenderers = new List<MeshRenderer>();
        private MaterialPropertyBlock shieldTint;
        private bool shieldTinted;
        private MeshRenderer parrySparkRenderer;
        private Mesh parrySparkMesh;
        private Material parrySparkMaterial;
        private bool downed;
        private float defeatStarted = -1f;
        private Quaternion defeatStartRotation;

        public void SetWeaponVisible(bool visible)
        {
            Configure();
            if (visual == null) return;
            foreach (var renderer in visual.GetComponentsInChildren<MeshRenderer>(true))
                if (renderer.name.StartsWith("Sword", System.StringComparison.Ordinal)) renderer.enabled = visible;
        }

        public void Configure()
        {
            if (visual != null) return;
            visual = transform.Find("V01Hero");
            if (visual == null) visual = transform.Find("GuardVisual");
            if (visual == null)
            {
                // Monster prefabs have static mesh parts directly below their network root.
                // Only those parts move: HUDs, telegraphs and other runtime children stay put.
                var parts = new List<Transform>();
                foreach (Transform child in transform)
                    if (child.GetComponent<MeshRenderer>() != null) parts.Add(child);
                if (parts.Count == 0) return;
                visual = new GameObject("MotionVisual").transform;
                visual.SetParent(transform, false);
                foreach (var part in parts) part.SetParent(visual, true);
            }

            restPosition = visual.localPosition;
            restRotation = visual.localRotation;
            height = 0;
            foreach (var renderer in visual.GetComponentsInChildren<MeshRenderer>())
            {
                float top = visual.InverseTransformPoint(renderer.bounds.max).y;
                float bottom = visual.InverseTransformPoint(renderer.bounds.min).y;
                height = Mathf.Max(height, top - Mathf.Min(0, bottom));
            }
            height = Mathf.Max(height, 1f);

            bool guard = visual.Find("Breastplate") != null;
            guardMotion = guard;
            string kind = guard ? "Guard" : visual.Find("GoblinTorso") != null ? "Goblin" :
                visual.Find("OrcTorso") != null ? "Orc" : visual.Find("GolemTorso") != null ? "Golem" : null;
            if (kind != null)
            {
                for (int side = 0; side < 2; side++)
                {
                    int sign = side == 0 ? -1 : 1;
                    Vector3 shoulder = kind == "Guard" ? new Vector3(sign * .27f, 1.62f, 0) :
                        kind == "Goblin" ? new Vector3(sign * .19f, 1.10f, -.05f) :
                        kind == "Orc" ? new Vector3(sign * .34f, 1.68f, -.02f) : new Vector3(sign * .69f, 2.46f, 0);
                    Vector3 hip = kind == "Guard" ? new Vector3(sign * .115f, 1.025f, .01f) :
                        kind == "Goblin" ? new Vector3(sign * .09f, .69f, .015f) :
                        kind == "Orc" ? new Vector3(sign * .15f, 1.03f, .01f) : new Vector3(sign * .28f, 1.44f, .04f);
                    arms[side] = CreateJoint(kind, sign, true, shoulder);
                    legs[side] = CreateJoint(kind, sign, false, hip);
                }
            }
            if (guardMotion)
            {
                shieldTint = new MaterialPropertyBlock();
                foreach (var renderer in visual.GetComponentsInChildren<MeshRenderer>())
                    if (renderer.name.Contains("Shield")) shieldRenderers.Add(renderer);
                upperBody = new GameObject("UpperBody").transform;
                upperBody.SetParent(visual, false);
                upperBody.localPosition = new Vector3(0f, 1.025f, 0f);
                var upperParts = new List<Transform>();
                foreach (Transform child in visual)
                    if (child != upperBody && child != legs[0] && child != legs[1]) upperParts.Add(child);
                foreach (var part in upperParts) part.SetParent(upperBody, true);
            }
            ResetPose();
        }

        private Transform CreateJoint(string kind, int sign, bool arm, Vector3 position)
        {
            var parts = new List<Transform>();
            string suffix = sign.ToString();
            foreach (Transform part in visual)
            {
                string name = part.name;
                bool matches;
                if (kind == "Guard")
                {
                    matches = arm ? name == "MailSleeve" + suffix || name == "UpperArmPlate" + suffix ||
                        name == "ElbowCop" + suffix || name == "Vambrace" + suffix ||
                        name == "GauntletCuff" + suffix || name == "Glove" + suffix ||
                        name.StartsWith("ShoulderLame" + suffix, System.StringComparison.Ordinal) ||
                        sign == 1 && name.StartsWith("Sword", System.StringComparison.Ordinal) ||
                        sign == -1 && name.StartsWith("Shield", System.StringComparison.Ordinal) :
                        name == "Trouser" + suffix || name == "Cuissard" + suffix ||
                        name == "KneeCop" + suffix || name == "Greave" + suffix ||
                        name == "Boot" + suffix || name == "Sabatons" + suffix;
                }
                else
                {
                    matches = arm ? name == kind + "UpperArm" + suffix || name == kind + "Forearm" + suffix ||
                        name == kind + "Hand" + suffix || name == kind + "WristWrap" + suffix ||
                        name == kind + "Bracer" + suffix || name == kind + "ShoulderMoss" + suffix ||
                        kind == "Orc" && sign == -1 && name == "OrcPauldron" ||
                        sign == 1 && (name == kind + "Dagger" || name == kind + "DaggerGrip" ||
                            name == kind + "AxeShaft" || name == kind + "AxeHead") :
                        name == kind + "Thigh" + suffix || name == kind + "Shin" + suffix ||
                        name == kind + "Boot" + suffix || name == kind + "UpperLeg" + suffix ||
                        name == kind + "LowerLeg" + suffix || name == kind + "Foot" + suffix;
                }
                if (matches) parts.Add(part);
            }
            if (parts.Count == 0) return null;
            var joint = new GameObject((arm ? "Shoulder" : "Hip") + suffix).transform;
            joint.SetParent(visual, false);
            joint.localPosition = position;
            // Some mesh vertices include the authored body coordinates. Preserve their world pose.
            foreach (var part in parts) part.SetParent(joint, true);
            return joint;
        }

        // Local input owns facing; reconciliation displacement must never become a walking direction.
        // Observed actors continue to derive locomotion from their interpolated position.
        public void SetMovementIntent(Vector2 direction)
        {
            useMovementIntent = true;
            movementIntent = !float.IsNaN(direction.x) && !float.IsInfinity(direction.x) &&
                !float.IsNaN(direction.y) && !float.IsInfinity(direction.y) ? direction.normalized : Vector2.zero;
        }

        // Call only for accepted server action phases; this does not send or confirm damage.
        public void PlayAttack(byte direction, bool released, byte skillSlot = 0, float chargeDuration = 0)
        {
            if (direction > 3) return;
            PlayAttack(direction == 0 ? 0f : direction == 1 ? 180f : direction == 2 ? 270f : 90f,
                released, skillSlot, chargeDuration);
        }

        public void PlayAttack(float aimAngle, bool released, byte skillSlot = 0, float chargeDuration = 0)
        {
            if (visual == null || float.IsNaN(aimAngle) || float.IsInfinity(aimAngle) || aimAngle < 0 || aimAngle >= 360 ||
                downed || defeatStarted >= 0) return;
            float now = Time.unscaledTime;
            byte nextAttackSlot = skillSlot <= 2 ? skillSlot : (byte)0;
            if (guardMotion && !this.released && released && now < attackUntil && nextAttackSlot == attackSlot)
            {
                // Keep the accepted charging pose as the release clip's starting pose.
                // Monster motion remains on its existing phase transition path.
                for (int side = 0; side < 2; side++)
                {
                    phaseStartArms[side] = arms[side] != null ? arms[side].localRotation : Quaternion.identity;
                    phaseStartLegs[side] = legs[side] != null ? legs[side].localRotation : Quaternion.identity;
                }
                phaseStartVisualRotation = visual.localRotation;
                phaseStartUpperBodyRotation = upperBody != null ? upperBody.localRotation : Quaternion.identity;
                phaseTransitionStarted = now;
                phaseTransitionUntil = now + .1f;
            }
            else phaseTransitionUntil = 0;
            this.released = released;
            attackSlot = nextAttackSlot;
            spinPendingCompletion = guardMotion && released && attackSlot == 1;
            attackStarted = Time.unscaledTime;
            attackUntil = attackStarted + (released ? (attackSlot == 2 ? .58f : .48f) : Mathf.Max(.52f, chargeDuration + .08f));
            // Authored meshes face -Z; the wire angle measures clockwise from world +Z.
            facing = Quaternion.Euler(0, aimAngle + 180f, 0);
            hasFacing = true;
        }

        // Call on an observed HP decrease, never on the first health snapshot.
        public void PlayHit()
        {
            if (visual != null && !downed && defeatStarted < 0) hitUntil = Time.unscaledTime + HitDurationSeconds;
        }

        // Broadcast presentation only. Neither this pose nor a late acknowledgement grants protection.
        public void PlayParry(bool success, float windowSeconds)
        {
            if (visual == null || downed || defeatStarted >= 0 || float.IsNaN(windowSeconds) ||
                float.IsInfinity(windowSeconds) || windowSeconds <= 0) return;
            float now = Time.unscaledTime;
            if (success)
            {
                if (parrySucceeded) return;
                parrySucceeded = true;
                parryUntil = 0;
                parrySuccessUntil = now + ParrySuccessDuration;
                EnsureParrySparks();
            }
            else
            {
                if (now < parryUntil || now < parrySuccessUntil) return;
                parrySucceeded = false;
                parryStarted = now;
                parryUntil = now + windowSeconds;
            }
        }

        public void ClearParry()
        {
            parryUntil = parrySuccessUntil = 0;
            parrySucceeded = false;
            if (parrySparkRenderer != null) parrySparkRenderer.enabled = false;
            if (!shieldTinted) return;
            foreach (var renderer in shieldRenderers)
                if (renderer != null) renderer.SetPropertyBlock(null);
            shieldTinted = false;
        }

        private void EnsureParrySparks()
        {
            if (parrySparkRenderer != null || shieldRenderers.Count == 0 || arms[0] == null ||
                shieldRenderers[0] == null || shieldRenderers[0].sharedMaterial == null) return;
            // Reuse the knight's included material shader; no runtime-only shader lookup or new asset dependency.
            parrySparkMaterial = new Material(shieldRenderers[0].sharedMaterial) { name = "Parry Metal Sparks" };
            parrySparkMaterial.SetColor("_BaseColor", new Color(2.6f, 2.3f, 1.6f));
            if (parrySparkMaterial.HasProperty("_Metallic")) parrySparkMaterial.SetFloat("_Metallic", 0);
            if (parrySparkMaterial.HasProperty("_Cull")) parrySparkMaterial.SetFloat("_Cull", 0);
            var spark = new GameObject("Parry Shield Sparks");
            spark.transform.SetParent(arms[0], false);
            spark.transform.position = shieldRenderers[0].bounds.center;
            var vertices = new Vector3[64];
            var triangles = new int[96];
            for (int ray = 0; ray < 8; ray++)
            {
                float angle = ray * Mathf.PI * .25f;
                Vector3 direction = new Vector3(Mathf.Cos(angle), Mathf.Sin(angle), 0);
                for (int plane = 0; plane < 2; plane++)
                {
                    Vector3 width = (plane == 0 ? new Vector3(-direction.y, direction.x, 0) : Vector3.forward) * .022f;
                    int vertex = ray * 8 + plane * 4;
                    vertices[vertex] = direction * .09f - width;
                    vertices[vertex + 1] = direction * .09f + width;
                    vertices[vertex + 2] = direction * (ray % 2 == 0 ? .64f : .46f) + width * .12f;
                    vertices[vertex + 3] = direction * (ray % 2 == 0 ? .64f : .46f) - width * .12f;
                    int index = (ray * 2 + plane) * 6;
                    triangles[index] = triangles[index + 3] = vertex;
                    triangles[index + 1] = vertex + 1;
                    triangles[index + 2] = triangles[index + 4] = vertex + 2;
                    triangles[index + 5] = vertex + 3;
                }
            }
            parrySparkMesh = new Mesh { name = "Parry Shield Sparks Mesh", vertices = vertices, triangles = triangles };
            parrySparkMesh.RecalculateNormals();
            parrySparkMesh.RecalculateBounds();
            spark.AddComponent<MeshFilter>().sharedMesh = parrySparkMesh;
            parrySparkRenderer = spark.AddComponent<MeshRenderer>();
            parrySparkRenderer.sharedMaterial = parrySparkMaterial;
            parrySparkRenderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
            parrySparkRenderer.receiveShadows = false;
            parrySparkRenderer.enabled = false;
        }

        // A player stays observed at zero HP; retain this pose until authoritative recovery.
        public void SetDowned(bool value)
        {
            if (visual == null || downed == value || defeatStarted >= 0) return;
            Quaternion retainedFacing = facing;
            bool retainedHasFacing = hasFacing;
            ResetPose();
            facing = retainedFacing;
            hasFacing = retainedHasFacing;
            Quaternion localFacing = hasFacing ? Quaternion.Inverse(visual.parent.rotation) * facing : restRotation;
            visual.localRotation = localFacing;
            downed = value;
            if (!downed) return;
            visual.localRotation = localFacing * Quaternion.Euler(80f, 0, 12f);
            for (int side = 0; side < 2; side++)
                if (arms[side] != null)
                    arms[side].localRotation = Quaternion.Euler(25f, 0, side == 0 ? -25f : 25f);
        }

        // Only the removed monster's mesh parts remain for this brief presentation.
        public bool PlayDefeat()
        {
            ClearParry();
            if (visual == null) return false;
            if (defeatStarted >= 0) return true;
            defeatStarted = Time.unscaledTime;
            defeatStartRotation = visual.localRotation;
            attackUntil = hitUntil = 0;
            return true;
        }

        private void LateUpdate()
        {
            if (visual == null) return;
            if (downed)
            {
                previousPosition = transform.position;
                return;
            }
            if (defeatStarted >= 0)
            {
                float elapsed = Time.unscaledTime - defeatStarted;
                float fall = Mathf.SmoothStep(0, 1, Mathf.Clamp01(elapsed / .4f));
                visual.localRotation = defeatStartRotation * Quaternion.Euler(80f * fall, 0, 12f * fall);
                visual.localPosition = restPosition + Vector3.up * (Mathf.Sin(fall * Mathf.PI) * height * .04f);
                for (int side = 0; side < 2; side++)
                {
                    if (arms[side] != null) arms[side].localRotation = Quaternion.Slerp(arms[side].localRotation,
                        Quaternion.Euler(25f, 0, side == 0 ? -25f : 25f), Mathf.Clamp01(Time.unscaledDeltaTime * 12f));
                    if (legs[side] != null) legs[side].localRotation = Quaternion.Slerp(legs[side].localRotation,
                        Quaternion.identity, Mathf.Clamp01(Time.unscaledDeltaTime * 12f));
                }
                if (elapsed >= .7f) { gameObject.SetActive(false); Destroy(gameObject); }
                return;
            }
            Vector3 delta = transform.position - previousPosition;
            previousPosition = transform.position;
            delta.y = 0;
            float distance = delta.magnitude;
            Vector3 intent = new Vector3(movementIntent.x, 0, movementIntent.y);
            if (distance > Mathf.Max(4f, height * visual.lossyScale.y * 2f))
            {
                ResetPose();
                return;
            }

            float now = Time.unscaledTime;
            bool attacking = now < attackUntil;
            float dt = Mathf.Max(Time.unscaledDeltaTime, .0001f);
            float locomotionDistance = useMovementIntent ? Mathf.Max(0, Vector3.Dot(delta, intent)) : distance;
            float worldHeight = Mathf.Max(.5f, height * Mathf.Abs(visual.lossyScale.y));
            float targetWalk = Mathf.Clamp01(locomotionDistance / dt / (worldHeight * 1.5f));
            walking = Mathf.MoveTowards(walking, targetWalk, dt * 8f);
            if (locomotionDistance > .0001f)
            {
                // A full cycle includes both feet. Cap cadence so delayed snapshots/corrections
                // cannot produce the old rapid windmill gait at the faster running speed.
                stride += Mathf.Min(locomotionDistance / (worldHeight * 1.7f), dt * 2.6f) * Mathf.PI * 2f;
            }
            Vector3 facingDirection = useMovementIntent ? intent : delta;
            if (!attacking && facingDirection.sqrMagnitude > .00000001f)
            {
                // Authored models face -Z. Skill wire direction is unchanged.
                facing = Quaternion.LookRotation(-facingDirection.normalized, Vector3.up);
                hasFacing = true;
            }
            float swing = Mathf.Sin(stride) * walking;
            bool phaseTransition = guardMotion && now < phaseTransitionUntil;
            float phaseBlend = phaseTransition
                ? Mathf.SmoothStep(0, 1, Mathf.Clamp01((now - phaseTransitionStarted) / (phaseTransitionUntil - phaseTransitionStarted)))
                : 1f;
            float attack = attacking ? Mathf.Clamp01((now - attackStarted) / (attackUntil - attackStarted)) : 1f;
            Vector3 swordPose = Vector3.zero;
            Vector3 supportArmPose = Vector3.zero;
            Vector3 leftLegPose = Vector3.zero;
            Vector3 rightLegPose = Vector3.zero;
            float torsoTurn = 0;
            float torsoPitch = 0;
            float bodySpin = 0;
            if (attacking)
            {
                if (attackSlot == 2)
                {
                    // The rectangular skill is a deliberate overhead wind-up followed by a weighted cleave.
                    Vector3 overhead = new Vector3(-145, -8, 24);
                    Vector3 impact = new Vector3(78, 12, -22);
                    if (!released)
                    {
                        float windup = Mathf.SmoothStep(0, 1, Mathf.Clamp01(attack / .7f));
                        swordPose = overhead * windup;
                        torsoTurn = -18f * windup;
                        torsoPitch = 4f * windup;
                        supportArmPose = new Vector3(-34f, -8f, -18f) * windup;
                        leftLegPose = new Vector3(8f, 0f, -6f) * windup;
                        rightLegPose = new Vector3(-4f, 0f, 4f) * windup;
                    }
                    else if (attack < .38f)
                    {
                        float cleave = 1f - Mathf.Pow(1f - attack / .38f, 3f);
                        swordPose = Vector3.Lerp(overhead, impact, cleave);
                        torsoTurn = Mathf.Lerp(-18f, 22f, cleave);
                        torsoPitch = Mathf.Lerp(4f, -7f, cleave);
                        supportArmPose = Vector3.Lerp(new Vector3(-34f, -8f, -18f), new Vector3(22f, 0f, -24f), cleave);
                        leftLegPose = Vector3.Lerp(new Vector3(8f, 0f, -6f), new Vector3(-8f, 0f, -10f), cleave);
                        rightLegPose = Vector3.Lerp(new Vector3(-4f, 0f, 4f), new Vector3(7f, 0f, 6f), cleave);
                    }
                    else
                    {
                        float recovery = Mathf.SmoothStep(0, 1, (attack - .38f) / .62f);
                        swordPose = Vector3.Lerp(impact, Vector3.zero, recovery);
                        torsoTurn = Mathf.Lerp(22f, 0, recovery);
                        torsoPitch = Mathf.Lerp(-7f, 0f, recovery);
                        supportArmPose = Vector3.Lerp(new Vector3(22f, 0f, -24f), Vector3.zero, recovery);
                        leftLegPose = Vector3.Lerp(new Vector3(-8f, 0f, -10f), Vector3.zero, recovery);
                        rightLegPose = Vector3.Lerp(new Vector3(7f, 0f, 6f), Vector3.zero, recovery);
                    }
                }
                else if (attackSlot == 1)
                {
                    Vector3 guard = new Vector3(-78, -18, 32);
                    Vector3 sweepPose = new Vector3(-18, 14, 145);
                    if (!released)
                    {
                        float brace = Mathf.SmoothStep(0, 1, Mathf.Clamp01(attack / .65f));
                        swordPose = guard * brace;
                        torsoTurn = -10f * brace;
                        torsoPitch = 2f * brace;
                        supportArmPose = new Vector3(-42f, 0f, 18f) * brace;
                        leftLegPose = new Vector3(7f, 0f, -8f) * brace;
                        rightLegPose = new Vector3(-5f, 0f, 6f) * brace;
                    }
                    else if (attack < .62f)
                    {
                        float sweep = attack / .62f;
                        swordPose = Vector3.Lerp(guard, sweepPose, Mathf.SmoothStep(0, 1, sweep));
                        torsoTurn = 18f * Mathf.Sin(sweep * Mathf.PI);
                        torsoPitch = -3f * Mathf.Sin(sweep * Mathf.PI);
                        supportArmPose = Vector3.Lerp(new Vector3(-42f, 0f, 18f), new Vector3(28f, 0f, -34f), sweep);
                        leftLegPose = Vector3.Lerp(new Vector3(7f, 0f, -8f), new Vector3(-5f, 0f, -12f), sweep);
                        rightLegPose = Vector3.Lerp(new Vector3(-5f, 0f, 6f), new Vector3(8f, 0f, 8f), sweep);
                    }
                    else
                    {
                        float recovery = Mathf.SmoothStep(0, 1, (attack - .62f) / .38f);
                        swordPose = Vector3.Lerp(sweepPose, Vector3.zero, recovery);
                        torsoTurn = 4f * Mathf.Sin(recovery * Mathf.PI);
                        torsoPitch = 0f;
                        supportArmPose = Vector3.Lerp(new Vector3(28f, 0f, -34f), Vector3.zero, recovery);
                        leftLegPose = Vector3.Lerp(new Vector3(-5f, 0f, -12f), Vector3.zero, recovery);
                        rightLegPose = Vector3.Lerp(new Vector3(8f, 0f, 8f), Vector3.zero, recovery);
                    }
                    if (released && !guardMotion)
                        bodySpin = -360f * Mathf.SmoothStep(0, 1, attack);
                }
                else
                {
                    Vector3 ready = new Vector3(-62, -20, 26);
                    Vector3 followThrough = new Vector3(52, 24, -48);
                    if (!released)
                    {
                        float preparation = Mathf.SmoothStep(0, 1, Mathf.Clamp01(attack / .7f));
                        swordPose = ready * preparation;
                        torsoTurn = -12f * preparation;
                        torsoPitch = 3f * preparation;
                        supportArmPose = new Vector3(-26f, 0f, -12f) * preparation;
                        leftLegPose = new Vector3(5f, 0f, -5f) * preparation;
                        rightLegPose = new Vector3(-3f, 0f, 3f) * preparation;
                    }
                    else if (attack < .32f)
                    {
                        // A quick lateral cut with the shoulders and stance turning behind the blade.
                        float cut = 1f - Mathf.Pow(1f - attack / .32f, 3f);
                        swordPose = Vector3.Lerp(ready, followThrough, cut);
                        torsoTurn = Mathf.Lerp(-12, 16, cut);
                        torsoPitch = Mathf.Lerp(3f, -4f, cut);
                        supportArmPose = Vector3.Lerp(new Vector3(-26f, 0f, -12f), new Vector3(18f, 0f, -28f), cut);
                        leftLegPose = Vector3.Lerp(new Vector3(5f, 0f, -5f), new Vector3(-7f, 0f, -9f), cut);
                        rightLegPose = Vector3.Lerp(new Vector3(-3f, 0f, 3f), new Vector3(6f, 0f, 5f), cut);
                    }
                    else
                    {
                        float recovery = Mathf.SmoothStep(0, 1, (attack - .32f) / .68f);
                        swordPose = Vector3.Lerp(followThrough, Vector3.zero, recovery);
                        torsoTurn = Mathf.Lerp(16, 0, recovery);
                        torsoPitch = Mathf.Lerp(-4f, 0f, recovery);
                        supportArmPose = Vector3.Lerp(new Vector3(18f, 0f, -28f), Vector3.zero, recovery);
                        leftLegPose = Vector3.Lerp(new Vector3(-7f, 0f, -9f), Vector3.zero, recovery);
                        rightLegPose = Vector3.Lerp(new Vector3(6f, 0f, 5f), Vector3.zero, recovery);
                    }
                }
            }
            float hitTime = 1f - (hitUntil - now) / HitDurationSeconds;
            float hit = hitUntil > now ? hitTime < .18f
                ? Mathf.SmoothStep(0, 1, hitTime / .18f)
                : 1f - Mathf.SmoothStep(0, 1, (hitTime - .18f) / .82f) : 0;
            // The brief recovery is presentation only and does not extend the broadcast protection window.
            float parry = parryUntil > 0 ? Mathf.SmoothStep(0, 1, Mathf.Clamp01((now - parryStarted) / .06f)) *
                Mathf.Clamp01((parryUntil + .16f - now) / .16f) : 0;
            float parryFlash = Mathf.Clamp01((parrySuccessUntil - now) / ParrySuccessDuration);
            float parryRebound = parryFlash > 0 ? Mathf.Sin((1f - parryFlash) * Mathf.PI) : 0;
            float shieldPose = Mathf.Max(parry, parryFlash);
            float poseResponse = Mathf.Clamp01(dt * 20f);
            for (int side = 0; side < 2; side++)
            {
                float sign = side == 0 ? -1 : 1;
                float gaitWeight = attacking ? .35f : 1f;
                Vector3 legPose = side == 0 ? leftLegPose : rightLegPose;
                Quaternion targetLeg = Quaternion.Euler(swing * sign * 22f * gaitWeight + legPose.x,
                    legPose.y, legPose.z);
                if (phaseTransition) targetLeg = Quaternion.Slerp(phaseStartLegs[side], targetLeg, phaseBlend);
                if (legs[side] != null) legs[side].localRotation = Quaternion.Slerp(legs[side].localRotation, targetLeg, poseResponse);
                Vector3 armPose;
                if (attacking)
                    armPose = side == 1 ? swordPose : supportArmPose + new Vector3(-18f, 0f, -10f) * Mathf.Sin(attack * Mathf.PI);
                else
                    armPose = new Vector3(-swing * sign * 10f, 0f, 0f);
                armPose += (side == 0 ? new Vector3(-35f, -8f, -14f) : new Vector3(12f, 10f, 5f)) * hit;
                armPose += ((side == 0 ? new Vector3(-82f, -30f, -20f) : new Vector3(-20f, 0, 12f)) * shieldPose +
                    (side == 0 ? new Vector3(-28f, 24f, -24f) : new Vector3(12f, -8f, 4f)) * parryRebound) *
                    (attacking ? .55f : 1f);
                Quaternion targetArm = Quaternion.Euler(armPose);
                if (phaseTransition) targetArm = Quaternion.Slerp(phaseStartArms[side], targetArm, phaseBlend);
                if (arms[side] != null) arms[side].localRotation = Quaternion.Slerp(arms[side].localRotation, targetArm, poseResponse);
            }
            float bob = (1f - Mathf.Cos(stride * 2f)) * .004f * height * walking;
            Quaternion localFacing = hasFacing ? Quaternion.Inverse(visual.parent.rotation) * facing : restRotation;
            visual.localPosition = restPosition + Vector3.up * bob +
                localFacing * new Vector3(0, -hit * height * .02f, hit * height * .065f);
            bool directSpin = false;
            if (guardMotion && released && attackSlot == 1)
            {
                if (attacking)
                {
                    bodySpin = -360f * Mathf.SmoothStep(0, 1, attack);
                    spinPendingCompletion = true;
                    directSpin = true;
                }
                else if (spinPendingCompletion)
                {
                    bodySpin = -360f;
                    spinPendingCompletion = false;
                    directSpin = true;
                }
            }
            Quaternion targetVisualRotation = guardMotion ? localFacing :
                localFacing * Quaternion.Euler(hit * -18f + walking * 3f, torsoTurn, swing * .65f) *
                Quaternion.AngleAxis(bodySpin, Vector3.up);
            if (phaseTransition) targetVisualRotation = Quaternion.Slerp(phaseStartVisualRotation, targetVisualRotation, phaseBlend);
            if (directSpin)
                visual.localRotation = targetVisualRotation * Quaternion.AngleAxis(bodySpin, Vector3.up);
            else
                visual.localRotation = Quaternion.Slerp(visual.localRotation, targetVisualRotation, Mathf.Clamp01(dt * 18f));
            if (upperBody != null)
            {
                Quaternion upperPose = Quaternion.Euler(torsoPitch - hit * 18f - shieldPose * 10f + parryRebound * 8f + walking * 3f,
                    torsoTurn + hit * 6f + shieldPose * 14f - parryRebound * 22f,
                    swing * .65f - hit * 5f - shieldPose * 8f + parryRebound * 5f);
                if (phaseTransition) upperPose = Quaternion.Slerp(phaseStartUpperBodyRotation, upperPose, phaseBlend);
                upperBody.localRotation = Quaternion.Slerp(upperBody.localRotation, upperPose, poseResponse);
            }
            if (parryFlash > 0 && shieldRenderers.Count > 0)
            {
                foreach (var renderer in shieldRenderers)
                {
                    if (renderer == null || renderer.sharedMaterial == null) continue;
                    Color original = renderer.sharedMaterial.HasProperty("_BaseColor")
                        ? renderer.sharedMaterial.GetColor("_BaseColor") : renderer.sharedMaterial.color;
                    Color flash = Color.Lerp(original, new Color(3.4f, 3.1f, 2.2f), Mathf.Sqrt(parryFlash));
                    shieldTint.SetColor("_BaseColor", flash);
                    shieldTint.SetColor("_Color", flash);
                    renderer.SetPropertyBlock(shieldTint);
                }
                shieldTinted = true;
            }
            else if (shieldTinted)
            {
                foreach (var renderer in shieldRenderers)
                    if (renderer != null) renderer.SetPropertyBlock(null);
                shieldTinted = false;
            }
            if (parrySparkRenderer != null)
            {
                parrySparkRenderer.enabled = parryFlash > 0;
                if (parryFlash > 0)
                {
                    float sparkTime = 1f - parryFlash;
                    float sparkScale = sparkTime < .2f ? Mathf.Lerp(.45f, 1f, sparkTime / .2f) :
                        Mathf.Lerp(1f, .04f, (sparkTime - .2f) / .8f);
                    parrySparkRenderer.transform.localScale = Vector3.one * sparkScale;
                }
            }
        }

        public void ResetPose()
        {
            ClearParry();
            previousPosition = transform.position;
            walking = stride = attackStarted = attackUntil = hitUntil = 0;
            downed = false;
            defeatStarted = -1f;
            released = false;
            spinPendingCompletion = false;
            phaseTransitionStarted = phaseTransitionUntil = 0;
            if (visual == null) return;
            visual.localPosition = restPosition;
            visual.localRotation = restRotation;
            if (upperBody != null) upperBody.localRotation = Quaternion.identity;
            hasFacing = false;
            movementIntent = Vector2.zero;
            for (int side = 0; side < 2; side++)
            {
                if (arms[side] != null) arms[side].localRotation = Quaternion.identity;
                if (legs[side] != null) legs[side].localRotation = Quaternion.identity;
            }
        }

        private void OnEnable() => ResetPose();
        private void OnDisable() => ResetPose();
        private void OnDestroy()
        {
            if (parrySparkRenderer != null)
            {
                if (Application.isPlaying) Destroy(parrySparkRenderer.gameObject);
                else DestroyImmediate(parrySparkRenderer.gameObject);
            }
            if (parrySparkMesh != null)
            {
                if (Application.isPlaying) Destroy(parrySparkMesh);
                else DestroyImmediate(parrySparkMesh);
            }
            if (parrySparkMaterial != null)
            {
                if (Application.isPlaying) Destroy(parrySparkMaterial);
                else DestroyImmediate(parrySparkMaterial);
            }
        }
    }
}
