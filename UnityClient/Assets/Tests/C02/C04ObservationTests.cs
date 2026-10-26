using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using C01;
using C02;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using TMPro;
using UnityEngine;

namespace C02.Tests
{
    // Detached state fixtures. These do not count as real UI input, server observation, or account sessions.
    public sealed class C04ObservationTests
    {
        [Test]
        [Category("WorldMinimap")]
        public void MinimapRadarKeepsSelfCentredClipsCircularMarkersAndUsesNorthUpAtlasWithinBounds()
        {
            var map = ScriptableObject.CreateInstance<C20WorldMap>();
            var atlas = new Texture2D(4, 4);
            try
            {
                map.origin = new Vector2(-29, -28); map.tileSize = 2; map.width = 39; map.height = 40;
                map.walkable = new byte[map.width * map.height];
                for (int i = 0; i < map.walkable.Length; i++) map.walkable[i] = 1;
                using (var fixture = new ObservationFixture())
                {
                    var other = Player(42); other.x = 21; other.y = 22;
                    var corner = Player(43); corner.x = 46; corner.y = 47;
                    fixture.Players(100, Player(7), other, corner);
                    var monster = Monster(0, 0); monster.x = 1; monster.y = 2;
                    var rim = Monster(1, 0); rim.x = 50;
                    var distant = Monster(2, 0); distant.x = 51.1f;
                    fixture.Monsters(100, monster, rim, distant);
                    var minimap = fixture.CreateMinimap(map);
                    Set(minimap, "terrainAtlas", atlas);
                    var atlasBounds = new Rect(-29, -28, 60, 80);
                    Set(minimap, "atlasBounds", atlasBounds);
                    Assert.That(minimap.mainTexture, Is.SameAs(atlas));
                    using (var vertices = new UnityEngine.UI.VertexHelper())
                    {
                        PopulateMinimap(minimap, vertices);
                        AssertMinimapColorCentre(vertices, new Color32(74, 219, 115, 255), Vector2.zero);
                        AssertMinimapColorCentre(vertices, new Color32(174, 181, 184, 255), new Vector2(10, 10));
                        var vertex = new UnityEngine.UIVertex();
                        int grey = 0, red = 0, atlasVertices = 0;
                        for (int i = 0; i < vertices.currentVertCount; i++)
                        {
                            vertices.PopulateUIVertex(ref vertex, i);
                            Assert.That(((Vector2)vertex.position).magnitude, Is.LessThanOrEqualTo(40.001f),
                                "All terrain, ring and marker geometry remains inside the circular viewport.");
                            if (vertex.color.Equals(new Color32(174, 181, 184, 255))) grey++;
                            if (vertex.color.Equals(new Color32(235, 74, 64, 255))) red++;
                            if (vertex.uv0.x < 0) continue;
                            atlasVertices++;
                            Vector2 world = fixture.World.ServerPosition + (Vector2)vertex.position;
                            Assert.That(world.x, Is.InRange(atlasBounds.xMin - .001f, atlasBounds.xMax + .001f));
                            Assert.That(world.y, Is.InRange(atlasBounds.yMin - .001f, atlasBounds.yMax + .001f));
                            Assert.That(vertex.uv0.x, Is.EqualTo((world.x - atlasBounds.xMin) / atlasBounds.width).Within(.0001f));
                            Assert.That(vertex.uv0.y, Is.EqualTo((world.y - atlasBounds.yMin) / atlasBounds.height).Within(.0001f),
                                "Positive world Y maps toward the north/top of the atlas.");
                        }
                        Assert.That(grey, Is.EqualTo(12), "The diagonal square-corner player is outside the circular range.");
                        Assert.That(red, Is.GreaterThan(12), "The near-rim monster has a clipped circular dot.");
                        Assert.That(red, Is.LessThan(30), "The out-of-radius monster does not become an edge marker.");
                        Assert.That(atlasVertices, Is.GreaterThan(0));
                    }
                    var own = fixture.World.OwnFull; own.x += 5; own.y += 5; Set(fixture.World, "ownFull", own);
                    Set(minimap, "nextRefresh", 0f); Invoke(minimap, "Update");
                    using (var vertices = new UnityEngine.UI.VertexHelper())
                    {
                        PopulateMinimap(minimap, vertices);
                        AssertMinimapColorCentre(vertices, new Color32(74, 219, 115, 255), Vector2.zero);
                        AssertMinimapColorCentre(vertices, new Color32(174, 181, 184, 255), new Vector2(5, 5));
                    }
                    Set(minimap, "terrainAtlas", null);
                    using (var vertices = new UnityEngine.UI.VertexHelper())
                    {
                        PopulateMinimap(minimap, vertices);
                        bool navigationShown = false;
                        var vertex = new UnityEngine.UIVertex();
                        for (int i = 0; i < vertices.currentVertCount; i++)
                        {
                            vertices.PopulateUIVertex(ref vertex, i);
                            Assert.That(vertex.uv0.x, Is.LessThan(0), "No atlas texture is sampled in navigation fallback.");
                            navigationShown |= vertex.color.Equals(new Color32(79, 91, 72, 235));
                        }
                        Assert.That(navigationShown, Is.True, "A missing bake keeps the existing navigation background available.");
                    }
                }
            }
            finally { UnityEngine.Object.DestroyImmediate(atlas); UnityEngine.Object.DestroyImmediate(map); }
        }

        [Test]
        [Category("WorldMinimap")]
        public void MinimapClearsDepartedDefeatedAndStaleWorldObservationsWithoutHidingDeadSelf()
        {
            var map = ScriptableObject.CreateInstance<C20WorldMap>();
            try
            {
                map.origin = new Vector2(-50, -50); map.width = map.height = 120;
                map.walkable = new byte[map.width * map.height];
                using (var fixture = new ObservationFixture())
                {
                    fixture.Players(100, Player(42)); fixture.Monsters(100, Monster(0, 0));
                    var minimap = fixture.CreateMinimap(map);
                    var group = (CanvasGroup)Get(minimap, "panelGroup");
                    var players = new List<Vector2>(); var monsters = new List<Vector2>();
                    Assert.That(fixture.Observer.CopyMinimapPositions(players, monsters), Is.True);
                    Assert.That(players.Count, Is.EqualTo(1)); Assert.That(monsters.Count, Is.EqualTo(1));
                    fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(18) });
                    fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 2, fieldVal = Bits(19) });
                    fixture.Observer.CopyMinimapPositions(players, monsters);
                    Assert.That(players[0], Is.EqualTo(new Vector2(18, 12)));
                    Assert.That(monsters[0], Is.EqualTo(new Vector2(11, 19)));
                    fixture.RemovePlayer(42);
                    fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 });
                    fixture.Observer.CopyMinimapPositions(players, monsters);
                    Assert.That(players, Is.Empty); Assert.That(monsters, Is.Empty);
                    var own = fixture.World.OwnFull; own.hp = 0; Set(fixture.World, "ownFull", own);
                    Set(minimap, "nextRefresh", 0f); Invoke(minimap, "Update");
                    Assert.That(group.alpha, Is.EqualTo(1), "HP0 is not a reason to hide the local player's map.");
                    typeof(C02WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(fixture.World,
                        new object[] { C02Stage.TransitionPending });
                    ((Action)Get(fixture.World, "WorldStateChanged"))?.Invoke();
                    Assert.That(group.alpha, Is.Zero, "Transition hides the whole panel immediately.");
                    players.Add(Vector2.one); monsters.Add(Vector2.one);
                    Assert.That(fixture.Observer.CopyMinimapPositions(players, monsters), Is.False);
                    Assert.That(players, Is.Empty); Assert.That(monsters, Is.Empty);
                    typeof(C02WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(fixture.World,
                        new object[] { C02Stage.InWorld });
                    typeof(C02WorldScreen).GetProperty("ZoneID").GetSetMethod(true).Invoke(fixture.World, new object[] { (ushort)2 });
                    Assert.That(fixture.Observer.CopyMinimapPositions(players, monsters), Is.False, "Old ownership cannot leak positions.");
                    fixture.CloseWorld();
                    Assert.That(group.alpha, Is.Zero);
                    using (var vertices = new UnityEngine.UI.VertexHelper())
                    { PopulateMinimap(minimap, vertices); Assert.That(vertices.currentVertCount, Is.Zero); }
                    fixture.OpenWorld(); Set(minimap, "nextRefresh", 0f); Invoke(minimap, "Update");
                    Assert.That(group.alpha, Is.EqualTo(1), "The always-active panel can recover on the next world entry.");
                    fixture.DisableObserver();
                    Assert.That(fixture.Observer.CopyMinimapPositions(players, monsters), Is.False);
                    Assert.That(players, Is.Empty); Assert.That(monsters, Is.Empty);
                    minimap.enabled = false;
                    Assert.That(group.alpha, Is.Zero);
                }
            }
            finally { UnityEngine.Object.DestroyImmediate(map); }
        }

        private static void PopulateMinimap(WorldMinimap minimap, UnityEngine.UI.VertexHelper vertices) =>
            typeof(WorldMinimap).GetMethod("OnPopulateMesh", BindingFlags.Instance | BindingFlags.NonPublic,
                null, new[] { typeof(UnityEngine.UI.VertexHelper) }, null).Invoke(minimap, new object[] { vertices });

        private static void AssertMinimapColorCentre(UnityEngine.UI.VertexHelper vertices, Color32 color, Vector2 expected)
        {
            Vector2 centre = Vector2.zero;
            int count = 0;
            var vertex = new UnityEngine.UIVertex();
            for (int i = 0; i < vertices.currentVertCount; i++)
            {
                vertices.PopulateUIVertex(ref vertex, i);
                if (!vertex.color.Equals(color)) continue;
                Assert.That(vertex.uv0.x, Is.LessThan(0), "Overlay colours must not be multiplied by the terrain atlas.");
                centre += (Vector2)vertex.position; count++;
            }
            Assert.That(count, Is.GreaterThan(0));
            Assert.That(Vector2.Distance(centre / count, expected), Is.LessThan(.001f));
        }

#if UNITY_EDITOR
        [UnityEngine.TestTools.UnityTest]
        public IEnumerator FirstHuntFeedback_MonsterDefeatKeepsOnlyBriefVisualAndLiveFullResetsIt()
        {
            using (var fixture = new ObservationFixture())
            {
                var prefab = UnityEditor.AssetDatabase.LoadAssetAtPath<GameObject>("Assets/C02/Art/C04/MonsterType0.prefab");
                Assert.That(prefab, Is.Not.Null);
                fixture.UseMonsterPrefabs(prefab);
                var dead = Monster(0, 0); dead.hp = 0;
                fixture.Monsters(Time.realtimeSinceStartupAsDouble, dead);
                Assert.That(fixture.Count("monsters"), Is.Zero, "An initial dead Full never creates a defeat replay.");
                fixture.Monsters(Time.realtimeSinceStartupAsDouble, Monster(0, 0));
                var actor = fixture.Visual("monsters", (ushort)0);
                var visual = actor.transform.Find("MotionVisual");
                var motion = actor.GetComponent<C19ActorMotion>();
                Quaternion initial = visual.localRotation;
                Vector3 authority = actor.transform.position;
                Quaternion facing = actor.transform.rotation;
                fixture.MonsterDeltas(
                    new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 },
                    new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 50 });
                Assert.That(fixture.Count("monsters"), Is.Zero, "HP0 wins within the accepted batch and removes observation immediately.");
                Assert.That(actor.activeSelf, Is.True, "Only the short defeat visual remains.");
                Assert.That(actor.transform.Find("MonsterVitals").gameObject.activeSelf, Is.False);
                foreach (var collider in actor.GetComponentsInChildren<Collider>()) Assert.That(collider.enabled, Is.False);
                float started = (float)Get(motion, "defeatStarted");
                fixture.Monsters(Time.realtimeSinceStartupAsDouble, dead);
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 70 });
                Assert.That(fixture.Count("monsters"), Is.Zero, "Unknown Delta cannot respawn the removed actor.");
                Assert.That((float)Get(motion, "defeatStarted"), Is.EqualTo(started));
                yield return new WaitForSecondsRealtime(.2f);
                Assert.That(Quaternion.Angle(initial, visual.localRotation), Is.GreaterThan(15f));
                Assert.That(actor.transform.position, Is.EqualTo(authority));
                Assert.That(actor.transform.rotation, Is.EqualTo(facing));
                fixture.Monsters(Time.realtimeSinceStartupAsDouble, Monster(0, 0));
                var respawn = fixture.Visual("monsters", (ushort)0);
                Assert.That(respawn, Is.Not.SameAs(actor));
                Assert.That(actor.activeSelf, Is.False, "The live Full immediately clears the same-ID old visual.");
                Assert.That((float)Get(respawn.GetComponent<C19ActorMotion>(), "defeatStarted"), Is.LessThan(0));
                Assert.That(respawn.transform.Find("MonsterVitals").gameObject.activeSelf, Is.True);
                fixture.Monsters(Time.realtimeSinceStartupAsDouble, dead);
                Assert.That(fixture.Count("monsters"), Is.Zero, "An observed HP0 Full also starts defeat.");
                yield return new WaitForSecondsRealtime(.8f);
                Assert.That(respawn == null, Is.True, "The presentation cleans itself up within its bounded lifetime.");
                fixture.Monsters(Time.realtimeSinceStartupAsDouble, Monster(0, 0));
                var cleared = fixture.Visual("monsters", (ushort)0);
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 });
                fixture.Clear();
                Assert.That(cleared.activeSelf, Is.False, "World clearing also clears a pending defeat visual.");
                fixture.Monsters(Time.realtimeSinceStartupAsDouble, Monster(0, 0));
                var disabled = fixture.Visual("monsters", (ushort)0);
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 });
                fixture.DisableObserver();
                Assert.That(disabled.activeSelf, Is.False, "Disabling observation also clears pending presentation.");
            }
        }
#endif

#if UNITY_EDITOR
        [Test]
        [Category("PR55Followup")]
        public void NearbyCombatBundle_DownedRecoveryKeepsLatestFacingAndLifecycleResetsIt()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.UsePlayerPrefab(UnityEditor.AssetDatabase.LoadAssetAtPath<GameObject>("Assets/C02/Art/C04/ObservedPlayer.prefab"));
                fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42));
                var actor = fixture.Visual("players", (uint)42);
                var motion = actor.GetComponent<C19ActorMotion>();
                var body = (Transform)Get(motion, "visual");
                Quaternion rest = body.localRotation;
                // Consume the initial spawn displacement before testing an accepted attack heading.
                Invoke(motion, "LateUpdate");
                foreach (byte direction in new byte[] { 3, 1 })
                {
                    motion.PlayAttack(direction, true, 2);
                    Set(motion, "attackStarted", Time.unscaledTime - .15f);
                    Invoke(motion, "LateUpdate");
                    Quaternion expected = Quaternion.Inverse(body.parent.rotation) * Quaternion.Euler(0, direction == 3 ? 270 : 0, 0);
                    fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 0 });
                    Assert.That((bool)Get(motion, "hasFacing"), Is.True);
                    Assert.That(Quaternion.Angle(body.localRotation, expected), Is.GreaterThan(60));
                    if (direction == 3)
                        fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 30 });
                    else fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42));
                    Assert.That((bool)Get(motion, "downed"), Is.False);
                    Assert.That(Quaternion.Angle(body.localRotation, expected), Is.LessThan(.01f),
                        "Recovery restores upright facing, without reusing the attack or downed rotation.");
                    Invoke(motion, "LateUpdate");
                    Assert.That(Quaternion.Angle(body.localRotation, expected), Is.LessThan(.01f),
                        "Standing still after recovery keeps the last accepted heading.");
                }
                actor.SetActive(false);
                Assert.That((bool)Get(motion, "hasFacing"), Is.False);
                Assert.That(Quaternion.Angle(body.localRotation, rest), Is.LessThan(.01f));
                actor.SetActive(true);
                Assert.That((bool)Get(motion, "hasFacing"), Is.False);
                Assert.That(Quaternion.Angle(body.localRotation, rest), Is.LessThan(.01f),
                    "The normal disable/enable lifecycle still performs a full pose reset.");
            }
        }

        [UnityEngine.TestTools.UnityTest]
        public IEnumerator NearbyCombatBundle_DownedPlayersPersistRecoverAndCleanUpWithoutDefeatReplay()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.UsePlayerPrefab(UnityEditor.AssetDatabase.LoadAssetAtPath<GameObject>("Assets/C02/Art/C04/ObservedPlayer.prefab"));
                var dead = Player(42); dead.hp = 0;
                fixture.Players(Time.realtimeSinceStartupAsDouble, dead);
                var actor = fixture.Visual("players", (uint)42);
                var motion = actor.GetComponent<C19ActorMotion>();
                Assert.That((bool)Get(motion, "downed"), Is.True, "An initial zero-HP Full establishes the downed pose.");
                Assert.That((float)Get(motion, "defeatStarted"), Is.LessThan(0), "Player observation never starts monster disposal.");
                Assert.That((float)Get(actor.GetComponent<C13EntityVitals>(), "damagePulseUntil"), Is.Zero);
                var body = (Transform)Get(motion, "visual");
                Assert.That(body, Is.Not.Null);
                Assert.That(body.name, Is.EqualTo("GuardVisual"), "The real knight prefab uses its authored visual root.");
                var pose = body.localRotation;
                motion.PlayAttack(3, true, 2); motion.PlayHit();
                yield return new WaitForSecondsRealtime(.8f);
                Assert.That(actor != null && actor.activeSelf, Is.True);
                Assert.That(Quaternion.Angle(body.localRotation, pose), Is.LessThan(.01f));
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 30 });
                Assert.That(fixture.Visual("players", (uint)42), Is.SameAs(actor));
                Assert.That((bool)Get(motion, "downed"), Is.False);
                Assert.That(Quaternion.Angle(body.localRotation, pose), Is.GreaterThan(60));
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 0 });
                Assert.That((bool)Get(motion, "downed"), Is.True);
                fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42));
                Assert.That((bool)Get(motion, "downed"), Is.False, "Full recovery also restores the same actor.");
                fixture.Players(Time.realtimeSinceStartupAsDouble, dead);
                fixture.RemovePlayer(42);
                Assert.That(actor.activeSelf, Is.False);
                fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42));
                var renewed = fixture.Visual("players", (uint)42);
                Assert.That((bool)Get(renewed.GetComponent<C19ActorMotion>(), "downed"), Is.False);
                fixture.CloseWorld();
                Assert.That(renewed.activeSelf, Is.False);
            }
        }
#endif

        [Test]
        public void NearbyCombatBundle_RemoteSkillAreasUseAcceptedPhaseGeometryAndBoundedObservationLifetime()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.ConfigureMonsterRange();
                fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42));
                var action = new ActionResultField { casterType = 0, casterId = 42, skillSlot = 1, skillId = 1, skillPhase = 0, dir = 0, x = 20, y = 30 };
                fixture.MonsterActions(action);
                var area = fixture.PlayerRange(42);
                Assert.That(area, Is.Not.Null);
                var mesh = area.GetComponent<MeshFilter>().sharedMesh;
                Assert.That(mesh.vertices.Length, Is.EqualTo(41));
                Assert.That(mesh.vertices[0], Is.EqualTo(new Vector3(20, .06f, 30)));
                Assert.That(mesh.bounds.size.x, Is.EqualTo(16).Within(.001f));
                double deadline = fixture.PlayerRangeDeadline(42);
                var invalid = action; invalid.casterId = 7;
                fixture.MonsterActions(invalid);
                invalid.casterId = 999; fixture.MonsterActions(invalid);
                invalid = action; invalid.skillId = 2; fixture.MonsterActions(invalid);
                invalid = action; invalid.dir = 4; fixture.MonsterActions(invalid);
                invalid = action; invalid.x = float.NaN; fixture.MonsterActions(invalid);
                invalid = action; invalid.skillPhase = 99; fixture.MonsterActions(invalid);
                Assert.That(fixture.PlayerRangeDeadline(42), Is.EqualTo(deadline), "Rejected actions cannot refresh the area.");
                action.skillSlot = 2; action.skillId = 2;
                for (byte direction = 0; direction < 4; direction++)
                {
                    action.dir = direction;
                    fixture.MonsterActions(action);
                    mesh = area.GetComponent<MeshFilter>().sharedMesh;
                    Assert.That(mesh.vertices.Length, Is.EqualTo(4));
                    Assert.That(direction < 2 ? mesh.bounds.size.z : mesh.bounds.size.x, Is.EqualTo(12).Within(.001f));
                    Assert.That(direction < 2 ? mesh.bounds.size.x : mesh.bounds.size.z, Is.EqualTo(3).Within(.001f));
                    float signedReach = direction == 0 ? mesh.bounds.center.z - 30 : direction == 1 ? 30 - mesh.bounds.center.z :
                        direction == 2 ? 20 - mesh.bounds.center.x : mesh.bounds.center.x - 20;
                    Assert.That(signedReach, Is.EqualTo(6).Within(.001f));
                }
                action.skillPhase = 1;
                fixture.MonsterActions(action);
                Assert.That(mesh.colors[0].r, Is.GreaterThan(mesh.colors[0].b), "Accepted Hit changes charge blue into a short gold release.");
                fixture.Render(fixture.PlayerRangeDeadline(42) + .01);
                Assert.That(area.gameObject.activeSelf, Is.False);
                action.skillPhase = 0; fixture.MonsterActions(action);
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 0 });
                Assert.That(area.gameObject.activeSelf, Is.False);
                fixture.MonsterActions(action);
                Assert.That(area.gameObject.activeSelf, Is.False, "Dead casters cannot start another charge.");
                fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42)); fixture.MonsterActions(action);
                fixture.RemovePlayer(42);
                Assert.That(area.gameObject.activeSelf, Is.False, "AOI removal disposes the remote skill area.");
                fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42)); fixture.MonsterActions(action);
                area = fixture.PlayerRange(42);
                fixture.Clear();
                Assert.That(area.gameObject.activeSelf, Is.False);
                fixture.Players(Time.realtimeSinceStartupAsDouble, Player(42)); fixture.MonsterActions(action);
                area = fixture.PlayerRange(42); fixture.DisableObserver();
                Assert.That(area.gameObject.activeSelf, Is.False);
            }
        }

        [Test]
        public void NearbyPlayerVitals_FollowAcceptedHealthWithoutMonsterDeathAndResetWithObservation()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.Players(100, Player(7), Player(42));
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "The local player's HUD remains separate.");
                var visual = fixture.Visual("players", (uint)42);
                var vitals = visual.GetComponent<C13EntityVitals>();
                var fill = fixture.VitalsImage(visual);
                var normalColor = fill.color;
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(.7f).Within(.001f));
                Assert.That((float)Get(vitals, "damagePulseUntil"), Is.Zero, "Initial Full establishes a baseline.");

                var damaged = Player(42); damaged.hp = 60;
                fixture.Players(101, damaged);
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(.6f).Within(.001f));
                Assert.That(fill.color, Is.EqualTo(new Color(1f, .78f, .2f, 1f)), "Full damage pulses too.");
                Set(vitals, "damagePulseUntil", Time.unscaledTime - 1f);
                Invoke(vitals, "LateUpdate");
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 60 });
                Assert.That(fill.color, Is.EqualTo(normalColor), "A matching Delta does not replay damage.");
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 101 });
                Assert.That(fixture.PlayerState(42).hp, Is.EqualTo(60));
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(.6f).Within(.001f));
                Assert.That(fill.color, Is.EqualTo(normalColor), "Rejected HP cannot pulse.");

                fixture.PlayerDeltas(
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 40 },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 2, fieldVal = 200 });
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(.2f).Within(.001f));
                Assert.That(fill.color, Is.EqualTo(new Color(1f, .78f, .2f, 1f)), "Delta damage uses accepted max HP.");
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 0 });
                Assert.That(fixture.Count("players"), Is.EqualTo(1));
                Assert.That(visual.activeSelf, Is.True, "Remote HP0 does not apply monster despawn behavior.");
                Assert.That(fixture.VitalsRatio(visual), Is.Zero);
                Assert.That((float)Get(visual.GetComponent<C19ActorMotion>(), "defeatStarted"), Is.LessThan(0));

                Set(vitals, "damagePulseUntil", Time.unscaledTime - 1f);
                Invoke(vitals, "LateUpdate");
                fixture.Players(102, Player(42));
                Assert.That(fixture.Visual("players", (uint)42), Is.SameAs(visual));
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(.7f).Within(.001f));
                Assert.That(fill.color, Is.EqualTo(normalColor), "Recovery is not damage.");
                fixture.Expire(105);
                Assert.That(fixture.Count("players"), Is.Zero);
                Assert.That(visual.activeSelf, Is.False, "AOI expiry removes the bar with its actor.");
                fixture.Players(106, Player(42));
                var renewed = fixture.Visual("players", (uint)42);
                Assert.That(renewed, Is.Not.SameAs(visual));
                Assert.That((float)Get(renewed.GetComponent<C13EntityVitals>(), "damagePulseUntil"), Is.Zero);
                fixture.DisableObserver();
                Assert.That(renewed.activeSelf, Is.False, "Observer cleanup also removes player vitals.");
            }
        }

        [Test]
        public void MonsterPacketsUse25And8ByteFields_AllowIdZero_AndRejectMalformedCounts()
        {
            Assert.That(Marshal.SizeOf<MonsterFullField>(), Is.EqualTo(25));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("internalId").ToInt32(), Is.Zero);
            Assert.That(Marshal.OffsetOf<MonsterFullField>("hp").ToInt32(), Is.EqualTo(2));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("maxHp").ToInt32(), Is.EqualTo(6));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("x").ToInt32(), Is.EqualTo(14));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("dir").ToInt32(), Is.EqualTo(22));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("monsterId").ToInt32(), Is.EqualTo(23));
            Assert.That(Marshal.SizeOf<MonsterDeltaField>(), Is.EqualTo(8));
            Assert.That(Marshal.OffsetOf<MonsterDeltaField>("fieldVal").ToInt32(), Is.EqualTo(4));
            var full = Frame(9, Monster(0, 2));
            var decoded = Packet.DeserializeVariablePacket<MonsterFullField>(Packet.ExtractHeader(full), full);
            Assert.That(decoded.count, Is.EqualTo(1));
            Assert.That(decoded.items[0].internalId, Is.Zero);
            Assert.That(decoded.items[0].monsterId, Is.EqualTo(2));
            Assert.That(decoded.items[0].maxHp, Is.EqualTo(100));
            Assert.That(decoded.items[0].x, Is.EqualTo(11));
            var delta = Frame(10, new MonsterDeltaField { internalId = 0, fieldId = 2, fieldVal = Bits(13.25f) });
            var update = Packet.DeserializeVariablePacket<MonsterDeltaField>(Packet.ExtractHeader(delta), delta).items[0];
            Assert.That(update.internalId, Is.Zero);
            Assert.That(update.fieldId, Is.EqualTo(2));
            Assert.That(BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0), Is.EqualTo(13.25f));
            foreach (var frame in new[] { full, delta })
            {
                var truncated = new byte[frame.Length - 1];
                Array.Copy(frame, truncated, truncated.Length);
                BitConverter.GetBytes((uint)truncated.Length).CopyTo(truncated, 2);
                AssertMalformed(truncated, frame[6]);
                var exaggerated = (byte[])frame.Clone();
                exaggerated[8] = exaggerated[9] = 255;
                AssertMalformed(exaggerated, frame[6]);
            }
        }

        [Test]
        public void FullCreatesSeparateKinds_ExcludesOwn_AndOnlyFullRenewsThreeSecondLifetime()
        {
            using (var fixture = new ObservationFixture())
            {
                var player = Player(42); player.hp = 0;
                fixture.Players(100, Player(7), player);
                fixture.Monsters(100, Monster(0, 0), Monster(42, 1), Monster(43, 2));
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Own ID is excluded; player HP0 is not deletion.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(3), "Monster ID0 and types0/1/2 are valid; same numeric player ID is separate.");
                var monsterVisual = fixture.Visual("monsters", (ushort)0);
                Quaternion initialFacing = monsterVisual.transform.rotation;
                Assert.That(Quaternion.Angle(initialFacing, Quaternion.Euler(0, 180, 0)), Is.LessThan(.001f));
                var directionFull = Monster(0, 0); directionFull.dir = 3;
                fixture.Monsters(100, directionFull);
                Assert.That(fixture.MonsterState(0).dir, Is.EqualTo(3), "Accepted server direction remains available to state consumers.");
                Assert.That(Quaternion.Angle(monsterVisual.transform.rotation, initialFacing), Is.LessThan(.001f),
                    "A periodic Full cannot rotate the actor root underneath its smoothly turning visual.");
                fixture.Players(101);
                fixture.Monsters(101);
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(12) });
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 1, fieldVal = Bits(14) });
                Assert.That(fixture.PlayerState(42).x, Is.EqualTo(12));
                Assert.That(fixture.MonsterState(0).x, Is.EqualTo(14));
                fixture.Expire(102.999);
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Absence in one Full cannot remove an observation.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(3));
                fixture.Expire(103);
                Assert.That(fixture.Count("players"), Is.Zero, "Delta must not extend Full lifetime.");
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(15) });
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 50 });
                Assert.That(fixture.Count("players"), Is.Zero);
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.Players(104, Player(42));
                fixture.Monsters(104, Monster(0, 0));
                fixture.Players(105.5, Player(42));
                fixture.Monsters(105.5, Monster(0, 0));
                fixture.Expire(107.1);
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "A later valid Full renews the observation.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
            }
        }

        [Test]
        public void MonsterDeathHidesImmediately_AndOnlyLiveFullCanReappear()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.Monsters(100, Monster(0, 0));
                var visual = fixture.Visual("monsters", (ushort)0);
                fixture.MonsterDeltas(
                    new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 },
                    new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 50 });
                Assert.That(visual.activeSelf, Is.False);
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 1, fieldVal = Bits(25) });
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 99, fieldId = 0, fieldVal = 100 });
                var dead = Monster(0, 0); dead.hp = 0;
                fixture.Monsters(101, dead);
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.Monsters(102, Monster(0, 0));
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
                fixture.Monsters(103, Monster(0, 2));
                Assert.That(fixture.MonsterState(0).monsterId, Is.EqualTo(2), "A live Full owns the observed type.");
            }
        }

        [Test]
        [Category("HuntingState")]
        public void MonsterVitalsFollowValidatedServerHealthAndEndWithDeath()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.Monsters(100, Monster(0, 2));
                var visual = fixture.Visual("monsters", (ushort)0);
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(0.7f).Within(0.001f));

                var lowHealth = Monster(1, 2);
                lowHealth.hp = 1;
                lowHealth.maxHp = 1500;
                fixture.Monsters(100, lowHealth);
                var lowHealthFill = fixture.VitalsFill(fixture.Visual("monsters", (ushort)1));
                Assert.That(lowHealthFill.anchorMax.x, Is.EqualTo(1f / 1500f).Within(0.000001f),
                    "A positive low-HP ratio must retain a positive fill width.");
                Assert.That(lowHealthFill.rect.width, Is.GreaterThan(0));
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 1, fieldId = 0, fieldVal = 0 });

                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 25 });
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(0.25f).Within(0.001f));

                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 101 });
                Assert.That(fixture.MonsterState(0).hp, Is.EqualTo(25));
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(0.25f).Within(0.001f), "Malformed HP must leave both state and bar unchanged.");

                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 });
                Assert.That(fixture.Count("monsters"), Is.Zero);
                Assert.That(visual.activeSelf, Is.False);
            }
        }

        [Test]
        public void MonsterFeedback_DamagePulseUsesObservedDeltaAndRestoresForInvalidDeathAndDisable()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.Monsters(100, Monster(0, 0));
                var visual = fixture.Visual("monsters", (ushort)0);
                var fill = fixture.VitalsImage(visual);
                var healthColor = fill.color;
                Assert.That(healthColor, Is.EqualTo(new Color(0.35f, 0.85f, 0.32f, 1f)),
                    "The initial Full establishes HP without claiming observed damage.");
                Assert.That((float)Get(visual.GetComponent<C13EntityVitals>(), "damagePulseUntil"), Is.Zero);

                var damagedFull = Monster(0, 0); damagedFull.hp = 60;
                fixture.Monsters(100, damagedFull);
                Assert.That(fixture.VitalsRatio(visual), Is.EqualTo(0.6f).Within(0.001f));
                Assert.That(fill.color, Is.EqualTo(new Color(1f, 0.78f, 0.2f, 1f)),
                    "A Full that first reveals an HP decrease pulses just like an accepted Delta.");
                Set(visual.GetComponent<C13EntityVitals>(), "damagePulseUntil", Time.unscaledTime - 1f);
                Invoke(visual.GetComponent<C13EntityVitals>(), "LateUpdate");
                fixture.MonsterDeltaPacket(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 60 });
                fixture.Monsters(100, damagedFull);
                Assert.That(fill.color, Is.EqualTo(healthColor), "The matching Delta and same-HP Full do not replay damage.");
                Assert.That((float)Get(visual.GetComponent<C13EntityVitals>(), "damagePulseUntil"), Is.Zero);
                fixture.Monsters(100, Monster(0, 0));
                Assert.That(fill.color, Is.EqualTo(healthColor), "An HP increase is not damage.");

                fixture.MonsterDeltaPacket(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 50 });
                Assert.That(fixture.MonsterState(0).hp, Is.EqualTo(50));
                Assert.That(fill.color, Is.EqualTo(new Color(1f, 0.78f, 0.2f, 1f)), "Only the accepted server HP decrease pulses gold.");
                float firstDeadline = (float)Get(visual.GetComponent<C13EntityVitals>(), "damagePulseUntil");
                fixture.MonsterDeltaPacket(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 40 });
                float refreshedDeadline = (float)Get(visual.GetComponent<C13EntityVitals>(), "damagePulseUntil");
                Assert.That(refreshedDeadline, Is.GreaterThanOrEqualTo(firstDeadline),
                    "A later hit refreshes the pulse deadline.");
                var sameHealthFull = Monster(0, 0); sameHealthFull.hp = 40;
                fixture.Monsters(100, sameHealthFull);
                Assert.That((float)Get(visual.GetComponent<C13EntityVitals>(), "damagePulseUntil"), Is.EqualTo(refreshedDeadline),
                    "A same-HP Full retains authoritative fill without starting another pulse.");

                Set(visual.GetComponent<C13EntityVitals>(), "damagePulseUntil", Time.unscaledTime - 1f);
                Invoke(visual.GetComponent<C13EntityVitals>(), "LateUpdate");
                Assert.That(fill.color, Is.EqualTo(healthColor), "The normal green fill returns after the pulse deadline.");

                fixture.MonsterDeltaPacket(
                    new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 40 },
                    new MonsterDeltaField { internalId = 0, fieldId = 1, fieldVal = Bits(14) });
                Assert.That(fill.color, Is.EqualTo(healthColor), "Same HP and movement do not pulse.");
                fixture.MonsterDeltaPacket(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 101 });
                Assert.That(fill.color, Is.EqualTo(healthColor), "An invalid HP batch does not pulse.");

                fixture.MonsterDeltaPacket(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 });
                Assert.That(visual.activeSelf, Is.False);
                Assert.That(fill.color, Is.EqualTo(healthColor), "Removing a dead monster disables its vitals and restores green immediately.");

                fixture.Monsters(101, Monster(0, 0));
                var recreated = fixture.Visual("monsters", (ushort)0);
                fixture.MonsterDeltaPacket(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 60 });
                var recreatedVitals = recreated.GetComponent<C13EntityVitals>();
                var recreatedFill = fixture.VitalsImage(recreated);
                Assert.That(recreatedFill.color, Is.EqualTo(new Color(1f, 0.78f, 0.2f, 1f)));
                recreatedVitals.enabled = false;
                Assert.That(recreatedFill.color, Is.EqualTo(healthColor), "Disabling vitals ends the pulse immediately.");
            }
        }

        [Test]
        public void MonsterActions_FillChargingSkillAreasKeepPhasePositionsAndSkipBasicTelegraphs()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.ConfigureMonsterRange();
                fixture.Content.ring.phases[1].waitTicks = 6;
                fixture.Content.ring.phases[1].range1 = 2;
                fixture.Monsters(100, Monster(0, 0), Monster(1, 1), Monster(2, 2), Monster(7, 1));
                double receivedAt = Time.realtimeSinceStartupAsDouble;
                fixture.MonsterActions(
                    MonsterAction(0, 0, 0, 0, 50, 60),
                    MonsterAction(1, 0, 1, 0, -4, 9),
                    MonsterAction(2, 0, 0, 0, 5, 6),
                    MonsterAction(99, 0, 0, 0, 7, 8),
                    MonsterAction(7, 0, 1, 0, 1, 2));

                Assert.That(fixture.MonsterRange(0), Is.Null, "Skill 0 is the monster's basic attack: it animates without a ground-area telegraph.");
                var smallRing = fixture.MonsterRange(1);
                double smallRingDeadline = fixture.RangeDeadline(1);
                var circleMesh = smallRing.GetComponent<MeshFilter>().sharedMesh;
                Assert.That(smallRing, Is.Not.Null);
                Assert.That(smallRing.gameObject.activeSelf, Is.True);
                Assert.That(circleMesh.vertexCount, Is.EqualTo(41));
                Assert.That(circleMesh.triangles.Length, Is.EqualTo(120), "The circle is a filled triangle fan, not a boundary line.");
                Assert.That(smallRing.sharedMaterial, Is.SameAs(fixture.RangeMaterial));
                Assert.That(circleMesh.vertices[1].x, Is.EqualTo(-2).Within(0.001f));
                Assert.That(circleMesh.vertices[1].y, Is.EqualTo(0.06f).Within(0.001f));
                Assert.That(circleMesh.vertices[1].z, Is.EqualTo(9).Within(0.001f));
                float initialCharge = circleMesh.colors[0].a;
                Assert.That(smallRingDeadline - receivedAt, Is.InRange(0.39, 0.42), "The preview uses the authored six-tick Hit delay plus cleanup grace.");
                fixture.Render(receivedAt + .2);
                Assert.That(circleMesh.colors[0].a, Is.GreaterThan(initialCharge), "The translucent filled area brightens while its cast charges.");
                var sameNumericId = fixture.MonsterRange(7);
                Assert.That(sameNumericId, Is.Not.Null, "Monster and player IDs are separate namespaces even when the number matches.");
                Assert.That(sameNumericId.GetComponent<MeshFilter>().sharedMesh.vertices[1].x, Is.EqualTo(3).Within(0.001f));
                Assert.That(fixture.Count("monsters"), Is.EqualTo(4), "Unobserved monster IDs do not create observations.");
                var invalidAction = MonsterAction(1, 0, 1, 0, 50, 60); invalidAction.x = float.NaN;
                fixture.MonsterActions(invalidAction);
                invalidAction = MonsterAction(1, 0, 1, 0, 50, 60); invalidAction.dir = 4;
                fixture.MonsterActions(invalidAction);
                Assert.That(fixture.RangeDeadline(1), Is.EqualTo(smallRingDeadline), "Invalid coordinates and directions cannot renew a preview.");

                fixture.MonsterActions(MonsterAction(1, 0, 1, 1, -4, 9));
                Assert.That(smallRing.gameObject.activeSelf, Is.False, "The server Hit phase hides the charging preview.");
                fixture.MonsterActions(MonsterAction(1, 0, 1, 0, -4, 9));
                fixture.Render(fixture.RangeDeadline(1) + 0.01);
                Assert.That(smallRing.gameObject.activeSelf, Is.False, "The cleanup grace expires without becoming a damage or hit indicator.");

                fixture.MonsterDeltaPacket(new MonsterDeltaField { internalId = 1, fieldId = 0, fieldVal = 0 });
                Assert.That(smallRing.gameObject.activeInHierarchy, Is.False, "HP0 removal destroys the telegraph with its observed monster.");

                double bossFirstReceivedAt = Time.realtimeSinceStartupAsDouble;
                var bossFirstAction = MonsterAction(2, 0, 3, 0, 5, 6); bossFirstAction.dir = 3;
                fixture.MonsterActions(bossFirstAction);
                var bossFirst = fixture.BossFanPreview(2, false);
                double bossFirstDeadline = fixture.BossFanDeadline(2, false);
                Assert.That(bossFirst, Is.Not.Null);
                var fanMesh = bossFirst.GetComponent<MeshFilter>().sharedMesh;
                Assert.That(fanMesh.vertexCount, Is.EqualTo(36));
                Assert.That(fanMesh.triangles.Length, Is.EqualTo(96), "Both server-authored fan sectors have filled triangles.");
                Assert.That(bossFirst.sharedMaterial, Is.SameAs(fixture.RangeMaterial));
                Assert.That(fanMesh.vertices[0], Is.EqualTo(new Vector3(5, 0.06f, 6)));
                Assert.That(fanMesh.vertices[1].x, Is.EqualTo(5 + Mathf.Cos(30 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(fanMesh.vertices[1].z, Is.EqualTo(6 + Mathf.Sin(30 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(fanMesh.vertices[17].x, Is.EqualTo(5 + Mathf.Cos(150 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(fanMesh.vertices[19].z, Is.EqualTo(6 + Mathf.Sin(210 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(bossFirstDeadline - bossFirstReceivedAt, Is.InRange(1.09, 1.12));

                double bossSecondReceivedAt = Time.realtimeSinceStartupAsDouble;
                var bossSecondAction = MonsterAction(2, 0, 3, 2, 15, 16); bossSecondAction.dir = 1;
                fixture.MonsterActions(bossSecondAction);
                var bossSecond = fixture.BossFanPreview(2, true);
                Assert.That(bossSecond, Is.Not.Null);
                Assert.That(bossSecond, Is.Not.SameAs(bossFirst));
                Assert.That(bossFirst.gameObject.activeSelf, Is.True, "A second Charging phase preserves the first preview.");
                var secondFanMesh = bossSecond.GetComponent<MeshFilter>().sharedMesh;
                Assert.That(secondFanMesh.vertices[1].x, Is.EqualTo(15 + Mathf.Cos(330 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(secondFanMesh.vertices[17].z, Is.EqualTo(16 + Mathf.Sin(390 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(secondFanMesh.vertices[19].z, Is.EqualTo(16 + Mathf.Sin(150 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(secondFanMesh.vertices[35].x, Is.EqualTo(15 + Mathf.Cos(210 * Mathf.Deg2Rad) * 10).Within(0.001f));
                Assert.That(secondFanMesh.vertices[0].x, Is.EqualTo(15).Within(0.001f), "The preview stays anchored to this phase's server cast center.");
                Assert.That(fixture.BossFanDeadline(2, true) - bossSecondReceivedAt, Is.InRange(1.09, 1.12));

                fixture.MonsterActions(MonsterAction(2, 0, 3, 1, 5, 6));
                Assert.That(bossFirst.gameObject.activeSelf, Is.False, "Hit phase 1 removes only the matching first preview.");
                Assert.That(bossSecond.gameObject.activeSelf, Is.True);
                fixture.MonsterActions(MonsterAction(2, 0, 3, 0, 5, 6));
                Assert.That(bossFirst.gameObject.activeSelf, Is.True);
                fixture.MonsterActions(MonsterAction(2, 0, 4, 3, 15, 16));
                Assert.That(bossFirst.gameObject.activeSelf, Is.True, "Unsupported skill 4 cannot hide a skill 3 preview.");
                Assert.That(bossSecond.gameObject.activeSelf, Is.True);
                fixture.MonsterActions(MonsterAction(2, 0, 3, 3, 15, 16));
                Assert.That(bossSecond.gameObject.activeSelf, Is.False, "Hit phase 3 removes only the matching second preview.");
                fixture.MonsterActions(
                    MonsterAction(2, 0, 3, 0, 5, 6),
                    MonsterAction(2, 0, 3, 2, 15, 16));
                Assert.That(bossFirst.gameObject.activeSelf, Is.True);
                Assert.That(bossSecond.gameObject.activeSelf, Is.True);
                fixture.Render(Math.Max(fixture.BossFanDeadline(2, false), fixture.BossFanDeadline(2, true)) + 0.01);
                Assert.That(bossFirst.gameObject.activeSelf, Is.False, "The first preview expires after its authored Hit wait plus cleanup grace.");
                Assert.That(bossSecond.gameObject.activeSelf, Is.False, "The second preview keeps its own TTL and expires independently.");
                fixture.MonsterActions(bossFirstAction);
                Assert.That(bossFirst.gameObject.activeSelf, Is.True);

                var bossSquareFirstAction = MonsterAction(2, 0, 4, 0, -10, 20); bossSquareFirstAction.dir = 2;
                fixture.MonsterActions(bossSquareFirstAction);
                var squareOuter0 = fixture.BossSquarePreview(2, 0, false);
                Assert.That(squareOuter0, Is.Not.Null);
                Assert.That(squareOuter0.GetComponent<MeshFilter>().sharedMesh.vertexCount, Is.EqualTo(4));
                Assert.That(squareOuter0.GetComponent<MeshFilter>().sharedMesh.triangles.Length, Is.EqualTo(6));
                Assert.That(squareOuter0.sharedMaterial, Is.SameAs(fixture.RangeMaterial));
                Assert.That(squareOuter0.GetComponent<MeshFilter>().sharedMesh.vertices[0], Is.EqualTo(new Vector3(-12.5f, 0.06f, 17.5f)));
                Assert.That(fixture.BossSquareDeadline(2, 0) - Time.realtimeSinceStartupAsDouble, Is.InRange(1.09, 1.11));
                Assert.That(bossFirst.gameObject.activeSelf, Is.True, "Skill 4 does not replace the skill 3 preview.");

                var bossSquareSecondAction = MonsterAction(2, 0, 4, 2, 8, 9); bossSquareSecondAction.dir = 1;
                fixture.MonsterActions(bossSquareSecondAction);
                var squareOuter1 = fixture.BossSquarePreview(2, 1, false);
                var squareMesh1 = squareOuter1.GetComponent<MeshFilter>().sharedMesh;
                Assert.That(squareMesh1.vertexCount, Is.EqualTo(16));
                Assert.That(squareMesh1.triangles.Length, Is.EqualTo(24), "The filled square ring leaves the server-defined center hole open.");
                Assert.That(squareMesh1.vertices[0], Is.EqualTo(new Vector3(3, 0.06f, 4)));
                Assert.That(squareMesh1.vertices[3], Is.EqualTo(new Vector3(5.5f, 0.06f, 6.5f)));
                Assert.That(fixture.BossSquareDeadline(2, 1) - Time.realtimeSinceStartupAsDouble, Is.InRange(1.09, 1.11));

                var bossSquareFifthAction = MonsterAction(2, 0, 4, 8, 3, 4); bossSquareFifthAction.dir = 3;
                fixture.MonsterActions(bossSquareFifthAction);
                var squareOuter4 = fixture.BossSquarePreview(2, 4, false);
                var squareMesh4 = squareOuter4.GetComponent<MeshFilter>().sharedMesh;
                Assert.That(squareMesh4.vertices[0], Is.EqualTo(new Vector3(-9.5f, 0.06f, -8.5f)));
                Assert.That(squareMesh4.vertices[3], Is.EqualTo(new Vector3(-7, 0.06f, -6)));
                Assert.That(fixture.BossSquareDeadline(2, 4) - Time.realtimeSinceStartupAsDouble, Is.InRange(1.09, 1.11));
                Assert.That(squareMesh1.vertices[0].x, Is.EqualTo(3).Within(0.001f), "The square fill remains axis aligned and uses the ActionResult center despite dir changes.");

                fixture.MonsterActions(MonsterAction(2, 0, 4, 1, -10, 20));
                Assert.That(squareOuter0.gameObject.activeSelf, Is.False, "Hit phase 1 removes only the first square pair.");
                Assert.That(squareOuter1.gameObject.activeSelf, Is.True);
                Assert.That(squareOuter4.gameObject.activeSelf, Is.True);
                fixture.MonsterActions(MonsterAction(2, 0, 4, 3, 8, 9));
                Assert.That(squareOuter1.gameObject.activeSelf, Is.False);
                Assert.That(squareOuter4.gameObject.activeSelf, Is.True, "A Hit clears only its corresponding later pair.");
                fixture.MonsterActions(MonsterAction(2, 0, 4, 9, 3, 4));
                Assert.That(squareOuter4.gameObject.activeSelf, Is.False);
                Assert.That(bossFirst.gameObject.activeSelf, Is.True, "Skill 4 Hit phases leave skill 3 previews untouched.");

                fixture.MonsterActions(
                    MonsterAction(2, 0, 4, 0, -10, 20),
                    MonsterAction(2, 0, 4, 2, 8, 9));
                fixture.Render(Math.Max(fixture.BossSquareDeadline(2, 0), fixture.BossSquareDeadline(2, 1)) + 0.01);
                Assert.That(squareOuter0.gameObject.activeSelf, Is.False, "The first square pair expires from its own authored TTL.");
                Assert.That(squareOuter1.gameObject.activeSelf, Is.False, "The second square pair expires without extending the first.");
                fixture.MonsterActions(MonsterAction(2, 0, 4, 0, -10, 20));
                var clearedSquare = fixture.BossSquarePreview(2, 0, false).gameObject;
                Assert.That(clearedSquare.activeSelf, Is.True);
                fixture.MonsterActions(bossFirstAction);
                Assert.That(bossFirst.gameObject.activeSelf, Is.True);
                fixture.Monsters(101, Monster(0, 1));
                var oldSource = fixture.NetworkModel;
                var nextModel = fixture.NewModel();
                oldSource.ActionResultReceived(1, 1, 1, new[] { MonsterAction(0, 0, 1, 0, 20, 21) });
                Assert.That(fixture.QueuedCount, Is.EqualTo(1), "The old source callback must still be pending at replacement.");
                fixture.Bind(nextModel);
                Assert.That(bossFirst.gameObject.activeInHierarchy, Is.False, "Model replacement removes active boss previews.");
                Assert.That(clearedSquare.activeInHierarchy, Is.False, "Model replacement removes active square previews.");
                fixture.Monsters(101, Monster(0, 1));
                fixture.Drain();
                Assert.That(fixture.MonsterRange(0), Is.Null, "A queued result from the replaced model cannot add a preview.");

                fixture.MonsterActions(MonsterAction(0, 0, 1, 0, 20, 21));
                var clearedPreview = fixture.MonsterRange(0).gameObject;
                fixture.CloseWorld();
                Assert.That(clearedPreview.activeInHierarchy, Is.False, "World clear removes the per-observation preview.");
            }
        }

        [Test]
        public void InvalidEntitiesDoNotPoisonOtherIds_AndDeltaCoordinatesAndHpAreAtomic()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.Players(100, Player(42), Player(43));
                fixture.Monsters(100, Monster(0, 0), Monster(1, 1));
                var badPlayer = Player(42); badPlayer.x = float.NaN;
                fixture.Players(101, badPlayer);
                var badMonster = Monster(0, 0); badMonster.dir = 4;
                fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 3); fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 0); badMonster.maxHp = 0; fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 0); badMonster.hp = -1; fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 0); badMonster.y = float.PositiveInfinity; fixture.Monsters(101, badMonster);
                fixture.PlayerDeltas(
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(12) },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 8, fieldVal = Bits(13) },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 150 },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 2, fieldVal = 200 });
                Assert.That(fixture.PlayerState(42).hp, Is.EqualTo(150), "Validate HP with maxHP from the same packet.");
                Assert.That(fixture.PlayerState(42).x, Is.EqualTo(12));
                Assert.That(fixture.PlayerState(42).y, Is.EqualTo(13));
                fixture.PlayerDeltas(
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 6, fieldVal = 256 },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(50) },
                    new DeltaUpdateField { zoneInternalID = 43, fieldID = 7, fieldVal = Bits(15) });
                Assert.That(fixture.PlayerState(42).x, Is.EqualTo(12), "Raw direction256 cannot wrap to0.");
                Assert.That(fixture.PlayerState(43).x, Is.EqualTo(15));
                fixture.MonsterDeltas(
                    new MonsterDeltaField { internalId = 0, fieldId = 1, fieldVal = Bits(float.NaN) },
                    new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(16) });
                Assert.That(fixture.MonsterState(0).x, Is.EqualTo(11));
                Assert.That(fixture.MonsterState(1).x, Is.EqualTo(16));
                Assert.That(fixture.Count("monsters"), Is.EqualTo(2));
                fixture.Expire(103);
                Assert.That(fixture.Count("players"), Is.Zero, "Invalid Full and valid Delta never renew LastFullSeen.");
                Assert.That(fixture.Count("monsters"), Is.Zero);
            }
        }

        [Test]
        public void SessionWorldAndDisableInvalidateCapturedCallbacks_AndCurrentModelRebinds()
        {
            using (var fixture = new ObservationFixture())
            {
                var source = fixture.NewModel();
                fixture.Bind(source);
                var old = fixture.PlayerCallback;
                old(1, new[] { Player(42) });
                Assert.That(fixture.Count("players"), Is.EqualTo(1));
                fixture.CloseWorld();
                Assert.That(fixture.Count("players"), Is.Zero);
                old(1, new[] { Player(42) });
                Assert.That(fixture.Count("players"), Is.Zero);
                fixture.OpenWorld();
                old(1, new[] { Player(42) });
                Assert.That(fixture.Count("players"), Is.Zero, "A previously captured generation cannot populate a new world.");
                ((IViewModelNet)source).FullReceived(1, 1, 0, 0, false, 1, new[] { Player(43) });
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Current source callbacks must be reattached after a world reset.");
                var previousClient = fixture.PlayerCallback;
                var next = fixture.NewModel();
                fixture.Bind(next);
                previousClient(1, new[] { Player(44) });
                ((IViewModelNet)source).FullReceived(1, 1, 0, 0, false, 1, new[] { Player(45) });
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.Zero);
                ((IViewModelNet)next).MonsterFullReceived(1, 1, 0, 0, false, 1, new[] { Monster(0, 0) });
                fixture.Drain();
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
                var beforeDisable = fixture.MonsterCallback;
                fixture.DisableObserver();
                Assert.That(fixture.Count("monsters"), Is.Zero);
                beforeDisable(1, new[] { Monster(0, 0) });
                Assert.That(fixture.Count("monsters"), Is.Zero);
            }
        }

        [Test]
        public void DestroyedVisualsAndDisplayRoot_DoNotInterruptRemoveClearOrDisable()
        {
            foreach (bool clearBeforeDisable in new[] { false, true })
            {
                using (var fixture = new ObservationFixture())
                {
                    fixture.Players(100, Player(42));
                    fixture.Monsters(100, Monster(0, 0));
                    UnityEngine.Object.DestroyImmediate(fixture.Visual("players", (uint)42));
                    UnityEngine.Object.DestroyImmediate(fixture.Visual("monsters", (ushort)0));
                    Assert.DoesNotThrow(() => fixture.RemovePlayer(42));
                    Assert.DoesNotThrow(() => fixture.RemoveMonster(0));
                    Assert.That(fixture.Count("players"), Is.Zero);
                    Assert.That(fixture.Count("monsters"), Is.Zero);
                    fixture.Players(101, Player(42));
                    fixture.Monsters(101, Monster(0, 0));
                    fixture.DestroyDisplayRoot();
                    if (clearBeforeDisable) Assert.DoesNotThrow(fixture.Clear);
                    Assert.DoesNotThrow(fixture.DisableObserver);
                    Assert.That(fixture.Count("players"), Is.Zero);
                    Assert.That(fixture.Count("monsters"), Is.Zero);
                }
            }
        }

        [Test]
        public void InvalidMonsterDeltaAndDeath_AreRejectedAtomicallyInEitherOrder()
        {
            foreach (bool deathFirst in new[] { false, true })
            foreach (bool invalidDirection in new[] { false, true })
            {
                using (var fixture = new ObservationFixture())
                {
                    fixture.Monsters(100, Monster(0, 0), Monster(1, 1));
                    var invalid = new MonsterDeltaField
                    {
                        internalId = 0,
                        fieldId = invalidDirection ? (ushort)3 : (ushort)1,
                        fieldVal = invalidDirection ? 4U : Bits(float.NaN)
                    };
                    var death = new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 };
                    fixture.MonsterDeltas(deathFirst ? death : invalid, deathFirst ? invalid : death,
                        new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(15) });
                    Assert.That(fixture.Count("monsters"), Is.EqualTo(2));
                    Assert.That(fixture.MonsterState(0).hp, Is.EqualTo(70));
                    Assert.That(fixture.MonsterState(0).x, Is.EqualTo(11));
                    Assert.That(fixture.MonsterState(0).dir, Is.Zero);
                    Assert.That(fixture.Visual("monsters", (ushort)0).activeSelf, Is.True);
                    Assert.That(fixture.MonsterState(1).x, Is.EqualTo(15), "A different valid ID must still apply.");
                }
            }
        }

        [Test]
        public void QueuedPacketsKeepEnqueueSubscribers_AndCannotPopulateReopenedWorld()
        {
            using (var fixture = new ObservationFixture())
            {
                var model = fixture.NewModel();
                fixture.Bind(model);
                var net = (IViewModelNet)model;
                fixture.Players(100, Player(43));
                fixture.Monsters(100, Monster(1, 1));
                net.FullReceived(1, 1, 0, 0, false, 1, new[] { Player(42) });
                net.DeltaReceived(1, 1, 1, new[] { new DeltaUpdateField { zoneInternalID = 43, fieldID = 7, fieldVal = Bits(99) } });
                net.MonsterFullReceived(1, 1, 0, 0, false, 1, new[] { Monster(0, 0) });
                net.MonsterDeltaReceived(1, 1, 1, new[] { new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(99) } });
                Assert.That(fixture.QueuedCount, Is.EqualTo(4));
                fixture.CloseWorld();
                fixture.OpenWorld();
                fixture.Players(101, Player(43));
                fixture.Monsters(101, Monster(1, 1));
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Old queued Full cannot create an observation in the new scope.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
                Assert.That(fixture.PlayerState(43).x, Is.EqualTo(11), "Old queued Delta cannot modify a newly observed ID.");
                Assert.That(fixture.MonsterState(1).x, Is.EqualTo(11));
                net.FullReceived(1, 1, 0, 0, false, 1, new[] { Player(42) });
                net.DeltaReceived(1, 1, 1, new[] { new DeltaUpdateField { zoneInternalID = 43, fieldID = 7, fieldVal = Bits(14) } });
                net.MonsterFullReceived(1, 1, 0, 0, false, 1, new[] { Monster(0, 0) });
                net.MonsterDeltaReceived(1, 1, 1, new[] { new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(15) } });
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.EqualTo(2));
                Assert.That(fixture.Count("monsters"), Is.EqualTo(2));
                Assert.That(fixture.PlayerState(43).x, Is.EqualTo(14));
                Assert.That(fixture.MonsterState(1).x, Is.EqualTo(15));
            }
        }

        [Test]
        public void DisposedViewModelDropsQueuedAndNewSnapshotCallbacks()
        {
            var dispatcher = new QueuedDispatcher();
            var model = NewModel(dispatcher);
            int callbacks = 0;
            model.OnFullReceived += (_, __) => callbacks++;
            model.OnDeltaReceived += (_, __) => callbacks++;
            model.OnMonsterFullReceived += (_, __) => callbacks++;
            model.OnMonsterDeltaReceived += (_, __) => callbacks++;
            var net = (IViewModelNet)model;
            net.FullReceived(1, 1, 0, 0, false, 1, new[] { Player(42) });
            net.DeltaReceived(1, 1, 0, Array.Empty<DeltaUpdateField>());
            net.MonsterFullReceived(1, 1, 0, 0, false, 1, new[] { Monster(0, 0) });
            net.MonsterDeltaReceived(1, 1, 0, Array.Empty<MonsterDeltaField>());
            Assert.That(dispatcher.Count, Is.EqualTo(4));
            model.Dispose();
            dispatcher.Drain();
            net.FullReceived(1, 1, 0, 0, false, 1, new[] { Player(42) });
            net.DeltaReceived(1, 1, 0, Array.Empty<DeltaUpdateField>());
            net.MonsterFullReceived(1, 1, 0, 0, false, 1, new[] { Monster(0, 0) });
            net.MonsterDeltaReceived(1, 1, 0, Array.Empty<MonsterDeltaField>());
            Assert.That(callbacks, Is.Zero);
            Assert.That(dispatcher.Count, Is.Zero);
        }

        private static FullStateField Player(uint id) => new FullStateField { zoneInternalID = id, hp = 70, maxHp = 100, x = 11, y = 12, dir = 0 };
        private static MonsterFullField Monster(ushort id, ushort type) => new MonsterFullField { internalId = id, monsterId = type, hp = 70, maxHp = 100, x = 11, y = 12, dir = 0 };
        private static ActionResultField MonsterAction(uint id, byte skillSlot, uint skillId, ushort phase, float x, float y) =>
            new ActionResultField { casterType = 1, casterId = id, skillSlot = skillSlot, skillId = skillId, skillPhase = phase, dir = 0, x = x, y = y };
        private static uint Bits(float value) => BitConverter.ToUInt32(BitConverter.GetBytes(value), 0);

        private static byte[] Frame<T>(byte opcode, T state) where T : struct
        {
            int size = Marshal.SizeOf<T>();
            var bytes = new byte[20 + size];
            BitConverter.GetBytes((ushort)0xABCD).CopyTo(bytes, 0);
            BitConverter.GetBytes((uint)bytes.Length).CopyTo(bytes, 2);
            bytes[6] = opcode; bytes[7] = 1; bytes[8] = 1;
            BitConverter.GetBytes((ushort)1).CopyTo(bytes, 10);
            BitConverter.GetBytes(1UL).CopyTo(bytes, 12);
            IntPtr buffer = Marshal.AllocHGlobal(size);
            try { Marshal.StructureToPtr(state, buffer, false); Marshal.Copy(buffer, bytes, 20, size); }
            finally { Marshal.FreeHGlobal(buffer); }
            return bytes;
        }

        private static void AssertMalformed(byte[] frame, byte opcode)
        {
            if (opcode == 9) Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<MonsterFullField>(Packet.ExtractHeader(frame), frame));
            else Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<MonsterDeltaField>(Packet.ExtractHeader(frame), frame));
        }

        private static ViewModel NewModel(QueuedDispatcher dispatcher)
        {
            var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData", true), true);
            var model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.Instance | BindingFlags.NonPublic, null,
                new object[] { new NoNetwork(), data, new NullLogger(), dispatcher }, null);
            C08TestSession.Ready(model, dispatcher.Drain);
            return model;
        }

        private sealed class ObservationFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly C02WorldScreen world;
            private readonly C04ObservedWorld observer;
            private readonly QueuedDispatcher dispatcher = new QueuedDispatcher();
            private readonly List<ViewModel> models = new List<ViewModel>();
            private C14ServerContent rangeContent;
            public Material RangeMaterial { get; private set; }
            public C14ServerContent Content => rangeContent;

            public ObservationFixture()
            {
                root = new GameObject("C04 detached state fixture"); root.SetActive(false);
                var login = root.AddComponent<C01LoginScreen>();
                Set(login, "loginIdInput", Child("id").AddComponent<TMP_InputField>());
                Set(login, "passwordInput", Child("password").AddComponent<TMP_InputField>());
                Set(login, "loginButton", Button("login"));
                Set(login, "statusText", Text("login status"));
                Set(login, "characterListText", Text("list"));
                world = root.AddComponent<C02WorldScreen>();
                Set(world, "loginScreen", login);
                Set(world, "characterChoices", Array.Empty<UnityEngine.UI.Button>());
                Set(world, "enterButton", Button("enter"));
                Set(world, "disconnectButton", Button("disconnect"));
                Set(world, "entryStatusText", Text("entry"));
                Set(world, "worldStatusText", Text("world status"));
                Set(world, "selectionPanel", Child("selection"));
                Set(world, "background", Child("background"));
                var field = Child("field"); Set(world, "worldRoot", field);
                Set(world, "worldHud", Child("HUD"));
                Set(world, "ownCharacter", Child("own").transform);
                Set(world, "fieldPlane", Child("plane").transform);
                Set(world, "quarterViewCamera", Child("camera").transform);
                observer = root.AddComponent<C04ObservedWorld>();
                Set(observer, "loginScreen", login); Set(observer, "worldScreen", world);
                var display = Child("observations"); display.transform.SetParent(field.transform, false);
                Set(observer, "displayRoot", display.transform);
                Set(observer, "playerPrefab", Child("player visual template"));
                Set(observer, "monsterPrefabs", new[] { Child("type0 template"), Child("type1 template"), Child("type2 template") });
                root.SetActive(true);
                OpenWorld();
                Bind(NewModel());
            }

            public C02WorldScreen World => world;
            public C04ObservedWorld Observer => observer;
            public WorldMinimap CreateMinimap(C20WorldMap map)
            {
                var panel = Child("minimap panel");
                var group = panel.AddComponent<CanvasGroup>(); group.alpha = 0;
                var graphic = Child("minimap graphic"); graphic.SetActive(false);
                graphic.transform.SetParent(panel.transform, false);
                ((RectTransform)graphic.transform).sizeDelta = new Vector2(80, 80);
                var minimap = graphic.AddComponent<WorldMinimap>();
                Set(minimap, "worldScreen", world); Set(minimap, "observedWorld", observer);
                Set(minimap, "worldMap", map); Set(minimap, "panelGroup", group);
                graphic.SetActive(true);
                return minimap;
            }

            public ViewModel NewModel() { var model = C04ObservationTests.NewModel(dispatcher); models.Add(model); return model; }
            public void UseMonsterPrefabs(GameObject prefab) => Set(observer, "monsterPrefabs", new[] { prefab, prefab, prefab });
            public void UsePlayerPrefab(GameObject prefab) => Set(observer, "playerPrefab", prefab);
            public IViewModelNet NetworkModel => models[models.Count - 1];
            public void ConfigureMonsterRange()
            {
                rangeContent = ScriptableObject.CreateInstance<C14ServerContent>();
                RangeMaterial = new Material(Shader.Find("Universal Render Pipeline/Unlit"));
                Set(observer, "serverContent", rangeContent);
                Set(observer, "monsterRangeMaterial", RangeMaterial);
            }
            public void Bind(ViewModel model) => Invoke(observer, "ChangeClient", model);
            public void Drain() => dispatcher.Drain();
            public int QueuedCount => dispatcher.Count;
            public Action<ushort, FullStateField[]> PlayerCallback => (Action<ushort, FullStateField[]>)Get(observer, "playerFull");
            public Action<ushort, MonsterFullField[]> MonsterCallback => (Action<ushort, MonsterFullField[]>)Get(observer, "monsterFull");
            public void Players(double now, params FullStateField[] states) => Invoke(observer, "ApplyPlayerFull", (ushort)states.Length, states, now);
            public void Monsters(double now, params MonsterFullField[] states) => Invoke(observer, "ApplyMonsterFull", (ushort)states.Length, states, now);
            public void PlayerDeltas(params DeltaUpdateField[] updates) => Invoke(observer, "ApplyPlayerDelta", (ushort)updates.Length, updates);
            public void MonsterDeltas(params MonsterDeltaField[] updates) => Invoke(observer, "ApplyMonsterDelta", (ushort)updates.Length, updates);
            public void MonsterDeltaPacket(params MonsterDeltaField[] updates)
            {
                ((IViewModelNet)models[models.Count - 1]).MonsterDeltaReceived(1, 1, (ushort)updates.Length, updates);
                dispatcher.Drain();
            }
            public void MonsterActions(params ActionResultField[] results)
            {
                QueueMonsterActions(results);
                dispatcher.Drain();
            }
            public void QueueMonsterActions(params ActionResultField[] results) =>
                ((IViewModelNet)models[models.Count - 1]).ActionResultReceived(1, 1, (ushort)results.Length, results);
            public void Render(double now) => Invoke(observer, "RenderObservations", now);
            public MeshRenderer MonsterRange(ushort id) => PreviewRenderer(EntryField("monsters", id, "RangePreview"));
            public MeshRenderer PlayerRange(uint id) => PreviewRenderer(EntryField("players", id, "RangePreview"));
            public double PlayerRangeDeadline(uint id) => PreviewDeadline(EntryField("players", id, "RangePreview"));
            public double RangeDeadline(ushort id) => PreviewDeadline(EntryField("monsters", id, "RangePreview"));
            public MeshRenderer BossFanPreview(ushort id, bool second) =>
                PreviewRenderer(EntryField("monsters", id, second ? "BossFanPreviewSecond" : "BossFanPreviewFirst"));
            public double BossFanDeadline(ushort id, bool second) =>
                PreviewDeadline(EntryField("monsters", id, second ? "BossFanPreviewSecond" : "BossFanPreviewFirst"));
            public MeshRenderer BossSquarePreview(ushort id, int pair, bool inner) => inner ? null :
                PreviewRenderer(((Array)EntryField("monsters", id, "BossSquarePreviews")).GetValue(pair));
            public double BossSquareDeadline(ushort id, int pair) =>
                PreviewDeadline(((Array)EntryField("monsters", id, "BossSquarePreviews")).GetValue(pair));
            public void Expire(double now) => Invoke(observer, "Expire", now);
            public void RemovePlayer(uint id) => Invoke(observer, "RemovePlayer", id);
            public void RemoveMonster(ushort id) => Invoke(observer, "RemoveMonster", id, false);
            public void Clear() => Invoke(observer, "ClearObservations");
            public void DestroyDisplayRoot() => UnityEngine.Object.DestroyImmediate(((Transform)Get(observer, "displayRoot")).gameObject);
            public int Count(string name) => ((IDictionary)Get(observer, name)).Count;
            public FullStateField PlayerState(uint id) => (FullStateField)EntryField("players", id, "State");
            public MonsterFullField MonsterState(ushort id) => (MonsterFullField)EntryField("monsters", id, "State");
            public GameObject Visual(string name, object id) => (GameObject)EntryField(name, id, "Visual");
            public float VitalsRatio(GameObject visual)
            {
                return VitalsFill(visual).anchorMax.x;
            }
            public RectTransform VitalsFill(GameObject visual) =>
                (RectTransform)typeof(C13EntityVitals).GetField("fillTransform", BindingFlags.Instance | BindingFlags.NonPublic)
                    .GetValue(visual.GetComponent<C13EntityVitals>());
            public UnityEngine.UI.Image VitalsImage(GameObject visual) =>
                (UnityEngine.UI.Image)typeof(C13EntityVitals).GetField("fillImage", BindingFlags.Instance | BindingFlags.NonPublic)
                    .GetValue(visual.GetComponent<C13EntityVitals>());
            public void CloseWorld() => Invoke(world, "ResetWorld");
            public void DisableObserver() => observer.enabled = false;
            public void OpenWorld()
            {
                typeof(C02WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(world, new object[] { C02Stage.Entering });
                Invoke(world, "ZoneReceived", (ushort)1, (ulong)0, (uint)7, 10f, 10f);
                Invoke(world, "FullReceived", (ushort)1, new[] { Player(7) });
            }
            public void Dispose()
            {
                UnityEngine.Object.DestroyImmediate(root);
                foreach (var model in models) model.Dispose();
                if (RangeMaterial != null) UnityEngine.Object.DestroyImmediate(RangeMaterial);
                if (rangeContent != null) UnityEngine.Object.DestroyImmediate(rangeContent);
            }

            private object EntryField(string name, object id, string field)
            {
                var entry = ((IDictionary)Get(observer, name))[id];
                return entry.GetType().GetField(field).GetValue(entry);
            }
            private static MeshRenderer PreviewRenderer(object preview) => preview == null ? null :
                (MeshRenderer)preview.GetType().GetField("Renderer").GetValue(preview);
            private static double PreviewDeadline(object preview) => (double)preview.GetType().GetField("Deadline").GetValue(preview);
            private GameObject Child(string name) { var child = new GameObject(name, typeof(RectTransform)); child.transform.SetParent(root.transform, false); return child; }
            private UnityEngine.UI.Button Button(string name) => Child(name).AddComponent<UnityEngine.UI.Button>();
            private TMP_Text Text(string name) => Child(name).AddComponent<TextMeshProUGUI>();
        }

        private static object Get(object target, string field) => target.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static void Set(object target, string field, object value) => target.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static void Invoke(object target, string method, params object[] args) => target.GetType().GetMethod(method, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);
        private sealed class QueuedDispatcher : IMainThreadDispatcher
        {
            private readonly List<Action> jobs = new List<Action>();
            public int Count => jobs.Count;
            public void Post(Action job) => jobs.Add(job);
            public void Drain() { var pending = jobs.ToArray(); jobs.Clear(); foreach (var job in pending) job(); }
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
        private sealed class NoNetwork : INetworkService
        {
            public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { } public void RequestInventory() { } public void Profiles(uint[] profileIds) { }
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice) { }
            public void RequestWallet(uint requestID) { }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong character) { }
            public void Chat(string message, byte scope, ulong target) { }
            public void Action(byte direction, float x, float y, byte skill, ulong ownershipEpoch) { }
            public void Pong(ulong time) { }
            public void ZoneChange(byte operation) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { }
            public void Log(string message) { }
        }
    }
}
