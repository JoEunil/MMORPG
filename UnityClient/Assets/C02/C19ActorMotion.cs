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
        private float hitUntil;

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
        public void PlayAttack(byte direction, bool released)
        {
            if (visual == null || direction > 3) return;
            this.released = released;
            attackStarted = Time.unscaledTime;
            attackUntil = attackStarted + (released ? .32f : .55f);
            facing = C02WorldScreen.ServerFacing(direction);
            hasFacing = true;
        }

        // Call on an observed HP decrease, never on the first health snapshot.
        public void PlayHit()
        {
            if (visual != null) hitUntil = Time.unscaledTime + .2f;
        }

        private void LateUpdate()
        {
            if (visual == null) return;
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
            float targetWalk = Mathf.Clamp01(locomotionDistance / dt / Mathf.Max(1f, height));
            walking = Mathf.MoveTowards(walking, targetWalk, dt * 10f);
            if (locomotionDistance > .0001f)
            {
                stride += locomotionDistance / Mathf.Max(.5f, height * visual.lossyScale.y * .65f) * Mathf.PI * 2f;
            }
            Vector3 facingDirection = useMovementIntent ? intent : delta;
            if (!attacking && facingDirection.sqrMagnitude > .00000001f)
            {
                // Authored models face -Z. Skill wire direction is unchanged.
                facing = Quaternion.LookRotation(-facingDirection.normalized, Vector3.up);
                hasFacing = true;
            }
            float swing = Mathf.Sin(stride) * walking;
            float attack = attacking ? Mathf.Clamp01((now - attackStarted) / (attackUntil - attackStarted)) : 1f;
            float armAttack = !attacking ? 0 : released ? Mathf.Lerp(-70f, 65f, Mathf.Sin(attack * Mathf.PI)) * (1f - attack) :
                -65f * Mathf.SmoothStep(0, 1, Mathf.Min(1f, attack * 3));
            for (int side = 0; side < 2; side++)
            {
                float sign = side == 0 ? -1 : 1;
                if (legs[side] != null) legs[side].localRotation = Quaternion.Euler(swing * sign * 24f, 0, 0);
                if (arms[side] != null) arms[side].localRotation = Quaternion.Euler(
                    side == 1 && attacking ? armAttack : -swing * sign * 18f, 0,
                    side == 0 && attacking ? -12f * Mathf.Sin(attack * Mathf.PI) : 0);
            }
            float hit = hitUntil > now ? Mathf.Sin((hitUntil - now) / .2f * Mathf.PI) : 0;
            float bob = (1f - Mathf.Cos(stride * 2f)) * .008f * height * walking;
            visual.localPosition = restPosition + new Vector3(0, bob, hit * height * .06f);
            Quaternion lean = Quaternion.Euler(hit * -15f + walking * 2f, 0, swing * 1.4f);
            Quaternion localFacing = hasFacing ? Quaternion.Inverse(visual.parent.rotation) * facing : restRotation;
            visual.localRotation = Quaternion.Slerp(visual.localRotation, localFacing * lean, Mathf.Clamp01(dt * 18f));
        }

        public void ResetPose()
        {
            previousPosition = transform.position;
            walking = stride = attackStarted = attackUntil = hitUntil = 0;
            if (visual == null) return;
            visual.localPosition = restPosition;
            visual.localRotation = restRotation;
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
