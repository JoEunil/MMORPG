using System.Collections.Generic;
using UnityEngine;

namespace C02
{
    // Presentation only: the network/world components continue to own this actor's transform.
    [DisallowMultipleComponent]
    public sealed class C19ActorMotion : MonoBehaviour
    {
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
        private float defeatStarted = -1f;
        private Quaternion defeatStartRotation;

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
            if (visual == null || direction > 3 || defeatStarted >= 0) return;
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
            facing = C02WorldScreen.ServerFacing(direction);
            hasFacing = true;
        }

        // Call on an observed HP decrease, never on the first health snapshot.
        public void PlayHit()
        {
            if (visual != null && defeatStarted < 0) hitUntil = Time.unscaledTime + .2f;
        }

        // Only the removed monster's mesh parts remain for this brief presentation.
        public bool PlayDefeat()
        {
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
                Quaternion targetArm = Quaternion.Euler(armPose);
                if (phaseTransition) targetArm = Quaternion.Slerp(phaseStartArms[side], targetArm, phaseBlend);
                if (arms[side] != null) arms[side].localRotation = Quaternion.Slerp(arms[side].localRotation, targetArm, poseResponse);
            }
            float hit = hitUntil > now ? Mathf.Sin((hitUntil - now) / .2f * Mathf.PI) : 0;
            float bob = (1f - Mathf.Cos(stride * 2f)) * .004f * height * walking;
            visual.localPosition = restPosition + new Vector3(0, bob, hit * height * .025f);
            Quaternion localFacing = hasFacing ? Quaternion.Inverse(visual.parent.rotation) * facing : restRotation;
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
                localFacing * Quaternion.Euler(hit * -10f + walking * 3f, torsoTurn, swing * .65f) *
                Quaternion.AngleAxis(bodySpin, Vector3.up);
            if (phaseTransition) targetVisualRotation = Quaternion.Slerp(phaseStartVisualRotation, targetVisualRotation, phaseBlend);
            if (directSpin)
                visual.localRotation = targetVisualRotation * Quaternion.AngleAxis(bodySpin, Vector3.up);
            else
                visual.localRotation = Quaternion.Slerp(visual.localRotation, targetVisualRotation, Mathf.Clamp01(dt * 18f));
            if (upperBody != null)
            {
                Quaternion upperPose = Quaternion.Euler(torsoPitch + hit * -10f + walking * 3f, torsoTurn, swing * .65f);
                if (phaseTransition) upperPose = Quaternion.Slerp(phaseStartUpperBodyRotation, upperPose, phaseBlend);
                upperBody.localRotation = Quaternion.Slerp(upperBody.localRotation, upperPose, poseResponse);
            }
        }

        public void ResetPose()
        {
            previousPosition = transform.position;
            walking = stride = attackStarted = attackUntil = hitUntil = 0;
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
    }
}
