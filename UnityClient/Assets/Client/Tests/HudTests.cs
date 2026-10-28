using System;
using System.Reflection;
using ClientCore.PacketHelper;
using NUnit.Framework;
using TMPro;
using UnityEngine;

namespace Mmorpg.Client.Tests
{
    public sealed class HudTests
    {
        [Test]
        public void ServerFullAndVitalsDeltasUpdateBarsAndKeepExactText()
        {
            using (var fixture = new HudFixture())
            {
                Assert.That(fixture.Hp.fillAmount, Is.EqualTo(1));
                Assert.That(fixture.Mp.fillAmount, Is.EqualTo(1));
                Assert.That(fixture.Hp.type, Is.EqualTo(UnityEngine.UI.Image.Type.Filled));
                Assert.That(fixture.Mp.type, Is.EqualTo(UnityEngine.UI.Image.Type.Filled));
                Assert.That(fixture.Hp.sprite, Is.Not.Null, "A Filled uGUI Image needs a sprite to render its fill.");
                Assert.That(fixture.Mp.sprite, Is.Not.Null, "A Filled uGUI Image needs a sprite to render its fill.");

                fixture.Delta(
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 75 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 1, fieldVal = 25 });

                Assert.That(fixture.Hp.fillAmount, Is.EqualTo(.75f).Within(.001f));
                Assert.That(fixture.Mp.fillAmount, Is.EqualTo(.5f).Within(.001f));
                Assert.That(fixture.Vitals.text, Does.Contain("HP 75 / 100").And.Contain("MP 25 / 50"));
            }
        }

        [Test]
        public void HpZeroAndLogoutClearBarsAndReentryUsesTheNextFullState()
        {
            using (var fixture = new HudFixture())
            {
                Assert.That(fixture.Panel.activeSelf, Is.True);
                fixture.Full(new FullStateField
                {
                    zoneInternalID = 73, hp = 0, maxHp = 0, mp = 0, maxMp = 0,
                    x = 10, y = 10, dir = 3
                });
                Assert.That(fixture.Hp.fillAmount, Is.Zero);
                Assert.That(fixture.Mp.fillAmount, Is.Zero);
                Assert.That(fixture.Status.text, Does.Contain("HP 0"));
                Assert.That(fixture.HpValue.text, Is.EqualTo("HP 0 / 0"));
                Assert.That(fixture.MpValue.text, Is.EqualTo("MP 0 / 0"));
                Assert.That(fixture.Panel.activeSelf, Is.True, "Defeat keeps the server HP display visible.");

                fixture.Logout();
                Assert.That(fixture.Hp.fillAmount, Is.Zero);
                Assert.That(fixture.Mp.fillAmount, Is.Zero);
                Assert.That(fixture.Vitals.text, Is.Empty);
                Assert.That(fixture.HpValue.text, Is.Empty);
                Assert.That(fixture.MpValue.text, Is.Empty);
                Assert.That(fixture.Panel.activeSelf, Is.False);

                fixture.Reenter(new FullStateField
                {
                    zoneInternalID = 73, hp = 40, maxHp = 80, mp = 15, maxMp = 30,
                    x = 10, y = 10, dir = 2
                });
                Assert.That(fixture.Hp.fillAmount, Is.EqualTo(.5f).Within(.001f));
                Assert.That(fixture.Mp.fillAmount, Is.EqualTo(.5f).Within(.001f));
                Assert.That(fixture.Vitals.text, Does.Contain("HP 40 / 80").And.Contain("MP 15 / 30"));
                Assert.That(fixture.HpValue.text, Is.EqualTo("HP 40 / 80"));
                Assert.That(fixture.MpValue.text, Is.EqualTo("MP 15 / 30"));
                Assert.That(fixture.Panel.activeSelf, Is.True);
            }
        }

        [TestCase(1, 0, 0)]
        [TestCase(-1, 5, 0)]
        [TestCase(8, 5, 1)]
        public void GaugeRatioIsSafeForMissingMaximumAndClampedValues(int current, int maximum, float expected)
        {
            var method = typeof(CombatScreen).GetMethod("GaugeAmount", BindingFlags.Static | BindingFlags.NonPublic);
            Assert.That(method, Is.Not.Null);
            Assert.That((float)method.Invoke(null, new object[] { current, maximum }), Is.EqualTo(expected));
        }

        [Test]
        [Category("HuntingState")]
        public void OwnGrowthFullAndDeltasKeepRawServerValuesAndIgnoreOtherEntities()
        {
            using (var fixture = new HudFixture())
            {
                fixture.Full(new FullStateField
                {
                    zoneInternalID = 73, hp = 100, maxHp = 100, mp = 50, maxMp = 50,
                    level = 37, exp = 4000000001U, x = 10, y = 10, dir = 3
                });
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 37  EXP 4000000001"));

                fixture.Full(new FullStateField { zoneInternalID = 99, level = 1, exp = 2 });
                fixture.Delta(
                    new DeltaUpdateField { zoneInternalID = 99, fieldID = 4, fieldVal = 3 },
                    new DeltaUpdateField { zoneInternalID = 99, fieldID = 5, fieldVal = uint.MaxValue });
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 37  EXP 4000000001"));
                Assert.That(fixture.World.CanAct, Is.True);

                fixture.Delta(
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 4, fieldVal = uint.MaxValue },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 5, fieldVal = ushort.MaxValue },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 6, fieldVal = 2 });
                Assert.That(fixture.World.OwnFull.exp, Is.EqualTo(uint.MaxValue));
                Assert.That(fixture.World.OwnFull.level, Is.EqualTo(ushort.MaxValue));
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 65535  EXP 4294967295"));
                Assert.That(fixture.Vitals.text, Is.EqualTo("HP 100 / 100    MP 50 / 50"));
                Assert.That(fixture.Hp.fillAmount, Is.EqualTo(1));
                Assert.That(fixture.Mp.fillAmount, Is.EqualTo(1));
            }
        }

        [Test]
        [Category("HuntingState")]
        public void GrowthClearsOnLogoutThenUsesPendingEntryAndRejectsAnOverflowingLevel()
        {
            using (var fixture = new HudFixture())
            {
                fixture.Delta(
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 4, fieldVal = 90 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 5, fieldVal = 7 });
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 7  EXP 90"));
                fixture.Logout();
                fixture.Delta(
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 4, fieldVal = 999 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 5, fieldVal = 99 });
                Assert.That(fixture.Aim.text, Is.Empty);
                Assert.That(fixture.World.HasOwnFull, Is.False);

                fixture.ReenterPending(new[]
                {
                    new FullStateField { zoneInternalID = 99, level = 1, exp = 2 },
                    new FullStateField
                    {
                        zoneInternalID = 73, hp = 40, maxHp = 80, mp = 15, maxMp = 30,
                        level = 8, exp = 100, x = 10, y = 10, dir = 3
                    }
                },
                    new DeltaUpdateField { zoneInternalID = 99, fieldID = 5, fieldVal = 65536 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 4, fieldVal = 54321 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 5, fieldVal = 9 });
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 9  EXP 54321"));
                Assert.That(fixture.World.OwnFull.exp, Is.EqualTo(54321));
                Assert.That(fixture.World.OwnFull.level, Is.EqualTo(9));
                Assert.That(fixture.World.CanAct, Is.True, "Another pending entity's malformed level cannot abort own entry.");

                fixture.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 5, fieldVal = 65536 });
                Assert.That(fixture.World.Stage, Is.EqualTo(WorldStage.Failed));
                Assert.That(fixture.World.HasOwnFull, Is.False);
                Assert.That(fixture.World.CanAct, Is.False);
                Assert.That(fixture.Aim.text, Is.Empty);
                Assert.That(fixture.Vitals.text, Is.Empty);
                Assert.That(fixture.Hp.fillAmount, Is.Zero);
                Assert.That(fixture.Mp.fillAmount, Is.Zero);
            }
        }

        [Test]
        public void PlayerVitalsFeedback_ActualOwnChangesUpdateGrowthAndBrieflyEmphasizeHpLoss()
        {
            using (var fixture = new HudFixture())
            {
                Color hpColor = fixture.Hp.color;
                Color growthColor = fixture.Aim.color;
                Assert.That(fixture.Aim.gameObject.activeSelf, Is.True);
                Assert.That(fixture.Aim.rectTransform.anchorMin, Is.EqualTo(new Vector2(1, 0)));
                fixture.Delta(new DeltaUpdateField { zoneInternalID = 99, fieldID = 0, fieldVal = 5 },
                    new DeltaUpdateField { zoneInternalID = 99, fieldID = 4, fieldVal = 50 });
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.Zero);
                Assert.That(fixture.FeedbackDeadline("growthDeadline"), Is.Zero);
                fixture.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 75 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 4, fieldVal = uint.MaxValue },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 5, fieldVal = 8 });
                Assert.That(fixture.Hp.fillAmount, Is.EqualTo(.75f).Within(.001f));
                Assert.That(fixture.HpValue.text, Is.EqualTo("HP 75 / 100"));
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 8  EXP 4294967295"));
                Assert.That(fixture.Hp.color, Is.Not.EqualTo(hpColor));
                Assert.That(fixture.Aim.color, Is.Not.EqualTo(growthColor));
                float hpDeadline = fixture.FeedbackDeadline("hpLossDeadline");
                float growthDeadline = fixture.FeedbackDeadline("growthDeadline");
                fixture.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 75 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 1, fieldVal = 20 });
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.EqualTo(hpDeadline));
                Assert.That(fixture.FeedbackDeadline("growthDeadline"), Is.EqualTo(growthDeadline));
                fixture.ExpireFeedback(hpDeadline);
                Assert.That(fixture.Hp.color, Is.EqualTo(hpColor));
                fixture.ExpireFeedback(growthDeadline);
                Assert.That(fixture.Aim.color, Is.EqualTo(growthColor));
                fixture.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 90 });
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.Zero, "Recovery is not HP loss.");
            }
        }

        [Test]
        public void PlayerVitalsFeedback_EntryIdentityChangesAndLogoutResetBaselinesWithoutFalsePulse()
        {
            using (var fixture = new HudFixture())
            {
                Color hpColor = fixture.Hp.color;
                Color growthColor = fixture.Aim.color;
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.Zero, "The initial Full establishes the baseline.");
                fixture.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 60 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 4, fieldVal = 20 });
                fixture.Logout();
                Assert.That(fixture.Hp.color, Is.EqualTo(hpColor));
                Assert.That(fixture.Aim.color, Is.EqualTo(growthColor));
                fixture.ReenterPending(new[] { new FullStateField { zoneInternalID = 73, hp = 30, maxHp = 100,
                    mp = 20, maxMp = 50, level = 7, exp = 10, x = 10, y = 10, dir = 3 } },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 25 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 4, fieldVal = 15 });
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 7  EXP 15"));
                Assert.That(fixture.World.OwnFull.hp, Is.EqualTo(25));
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.Zero, "Bootstrap deltas establish the new session baseline.");
                Assert.That(fixture.FeedbackDeadline("growthDeadline"), Is.Zero);
                fixture.ChangeIdentity("SelectedCharacterID", 42UL);
                fixture.Full(new FullStateField { zoneInternalID = 73, hp = 20, maxHp = 100, mp = 20, maxMp = 50,
                    level = 3, exp = 2, x = 10, y = 10, dir = 3 });
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 3  EXP 2"));
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.Zero, "A different character is not a damage observation.");
                Assert.That(fixture.FeedbackDeadline("growthDeadline"), Is.Zero);
                fixture.ChangeIdentity("ZoneID", (ushort)2);
                fixture.ChangeIdentity("InternalID", 74U);
                fixture.Full(new FullStateField { zoneInternalID = 74, hp = 10, maxHp = 100, mp = 20, maxMp = 50,
                    level = 3, exp = 3, x = 210, y = 10, dir = 3 });
                Assert.That(fixture.World.CanAct, Is.True);
                Assert.That(fixture.World.OwnFull.hp, Is.EqualTo(10));
                Assert.That(fixture.Aim.text, Is.EqualTo("Lv 3  EXP 3"));
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.Zero, "A new zone/internal ID establishes its own baseline.");
                Assert.That(fixture.FeedbackDeadline("growthDeadline"), Is.Zero);
                Assert.That(fixture.Hp.color, Is.EqualTo(hpColor));
                fixture.Logout();
                Assert.That(fixture.Aim.text, Is.Empty);
                Assert.That(fixture.FeedbackDeadline("hpLossDeadline"), Is.Zero);
            }
        }

        private sealed class HudFixture : IDisposable
        {
            private readonly object fixture;
            private readonly object combat;
            private readonly object world;
            private readonly object model;
            private readonly GameObject hpObject;
            private readonly GameObject mpObject;
            private readonly Sprite gaugeSprite;
            public readonly UnityEngine.UI.Image Hp;
            public readonly UnityEngine.UI.Image Mp;
            public readonly TMP_Text Vitals;
            public readonly TMP_Text HpValue;
            public readonly TMP_Text MpValue;
            public readonly TMP_Text Status;
            public readonly TMP_Text Aim;
            public readonly GameObject Panel;
            public WorldScreen World => (WorldScreen)world;

            public HudFixture()
            {
                var fixtureType = typeof(CombatTests).GetNestedType("CombatFixture", BindingFlags.NonPublic);
                fixture = Activator.CreateInstance(fixtureType, true);
                combat = fixtureType.GetField("Combat", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                world = fixtureType.GetField("World", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                model = fixtureType.GetField("Model", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                Vitals = (TMP_Text)fixtureType.GetField("Vitals", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                HpValue = Text("HP value");
                MpValue = Text("MP value");
                Set(combat, "hpValueText", HpValue);
                Set(combat, "mpValueText", MpValue);
                Status = (TMP_Text)fixtureType.GetField("Status", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                Aim = (TMP_Text)combat.GetType().GetField("aimText", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(combat);

                hpObject = new GameObject("C12 HP fixture", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
                mpObject = new GameObject("C12 MP fixture", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
                Hp = hpObject.GetComponent<UnityEngine.UI.Image>();
                Mp = mpObject.GetComponent<UnityEngine.UI.Image>();
                gaugeSprite = Sprite.Create(Texture2D.whiteTexture, new Rect(0, 0, 1, 1), new Vector2(.5f, .5f));
                Hp.sprite = gaugeSprite;
                Mp.sprite = gaugeSprite;
                Hp.type = UnityEngine.UI.Image.Type.Filled;
                Mp.type = UnityEngine.UI.Image.Type.Filled;
                Set(combat, "hpFill", Hp);
                Set(combat, "mpFill", Mp);
                Panel = new GameObject("C12 separate panel fixture");
                HpValue.transform.SetParent(Panel.transform, false);
                MpValue.transform.SetParent(Panel.transform, false);
                Set(combat, "combatPanel", Panel);
                Invoke(combat, "WorldChanged");
            }

            public void Delta(params DeltaUpdateField[] updates) => Invoke(fixture, "Delta", new object[] { updates });

            public float FeedbackDeadline(string field) => (float)combat.GetType()
                .GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(combat);

            public void ExpireFeedback(float now) => Invoke(combat, "UpdateVitalsFeedback", now);

            public void ChangeIdentity(string property, object value) => typeof(WorldScreen).GetProperty(property)
                .GetSetMethod(true).Invoke(world, new[] { value });

            public void Full(FullStateField state) => Invoke(world, "FullReceived", (ushort)1, new[] { state });

            public void Logout()
            {
                Invoke(world, "ChangeClient", new object[] { null });
                Invoke(world, "ChangeClient", model);
            }

            public void Reenter(FullStateField state)
            {
                typeof(WorldScreen).GetProperty("Stage").GetSetMethod(true)
                    .Invoke(world, new object[] { WorldStage.Entering });
                Invoke(world, "ZoneReceived", (ushort)1, 0UL, 73U, 10f, 10f);
                Full(state);
            }

            public void ReenterPending(FullStateField[] states, params DeltaUpdateField[] updates)
            {
                typeof(WorldScreen).GetProperty("Stage").GetSetMethod(true)
                    .Invoke(world, new object[] { WorldStage.Entering });
                Invoke(world, "FullReceived", (ushort)states.Length, states);
                Delta(updates);
                Assert.That(World.Stage, Is.EqualTo(WorldStage.Entering));
                Assert.That(Aim.text, Is.Empty, "Pending Full and Delta cannot display before the matching Zone arrives.");
                Invoke(world, "ZoneReceived", (ushort)1, 0UL, 73U, 10f, 10f);
            }

            public void Dispose()
            {
                hpObject.transform.SetParent(null);
                mpObject.transform.SetParent(null);
                UnityEngine.Object.DestroyImmediate(gaugeSprite);
                UnityEngine.Object.DestroyImmediate(hpObject);
                UnityEngine.Object.DestroyImmediate(mpObject);
                Invoke(fixture, "Dispose");
                UnityEngine.Object.DestroyImmediate(Panel);
            }

            private static void Set(object target, string name, object value) =>
                target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);

            private static TMP_Text Text(string name) =>
                new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();

            private static void Invoke(object target, string name, params object[] args) =>
                target.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic).Invoke(target, args);
        }
    }
}
