using System;
using System.Reflection;
using ClientCore.PacketHelper;
using NUnit.Framework;
using TMPro;
using UnityEngine;

namespace C02.Tests
{
    public sealed class C12HudTests
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
                fixture.Full(new FullStateField
                {
                    zoneInternalID = 73, hp = 0, maxHp = 0, mp = 0, maxMp = 0,
                    x = 10, y = 10, dir = 3
                });
                Assert.That(fixture.Hp.fillAmount, Is.Zero);
                Assert.That(fixture.Mp.fillAmount, Is.Zero);
                Assert.That(fixture.Status.text, Does.Contain("HP 0"));

                fixture.Logout();
                Assert.That(fixture.Hp.fillAmount, Is.Zero);
                Assert.That(fixture.Mp.fillAmount, Is.Zero);
                Assert.That(fixture.Vitals.text, Is.Empty);

                fixture.Reenter(new FullStateField
                {
                    zoneInternalID = 73, hp = 40, maxHp = 80, mp = 15, maxMp = 30,
                    x = 10, y = 10, dir = 2
                });
                Assert.That(fixture.Hp.fillAmount, Is.EqualTo(.5f).Within(.001f));
                Assert.That(fixture.Mp.fillAmount, Is.EqualTo(.5f).Within(.001f));
                Assert.That(fixture.Vitals.text, Does.Contain("HP 40 / 80").And.Contain("MP 15 / 30"));
            }
        }

        [TestCase(1, 0, 0)]
        [TestCase(-1, 5, 0)]
        [TestCase(8, 5, 1)]
        public void GaugeRatioIsSafeForMissingMaximumAndClampedValues(int current, int maximum, float expected)
        {
            var method = typeof(C05CombatScreen).GetMethod("GaugeAmount", BindingFlags.Static | BindingFlags.NonPublic);
            Assert.That(method, Is.Not.Null);
            Assert.That((float)method.Invoke(null, new object[] { current, maximum }), Is.EqualTo(expected));
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
            public readonly TMP_Text Status;

            public HudFixture()
            {
                var fixtureType = typeof(C05CombatTests).GetNestedType("CombatFixture", BindingFlags.NonPublic);
                fixture = Activator.CreateInstance(fixtureType, true);
                combat = fixtureType.GetField("Combat", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                world = fixtureType.GetField("World", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                model = fixtureType.GetField("Model", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                Vitals = (TMP_Text)fixtureType.GetField("Vitals", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);
                Status = (TMP_Text)fixtureType.GetField("Status", BindingFlags.Instance | BindingFlags.Public).GetValue(fixture);

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
                Invoke(combat, "WorldChanged");
            }

            public void Delta(params DeltaUpdateField[] updates) => Invoke(fixture, "Delta", new object[] { updates });

            public void Full(FullStateField state) => Invoke(world, "FullReceived", (ushort)1, new[] { state });

            public void Logout()
            {
                Invoke(world, "ChangeClient", new object[] { null });
                Invoke(world, "ChangeClient", model);
            }

            public void Reenter(FullStateField state)
            {
                typeof(C02WorldScreen).GetProperty("Stage").GetSetMethod(true)
                    .Invoke(world, new object[] { C02Stage.Entering });
                Invoke(world, "ZoneReceived", (ushort)1, 0UL, 73U, 10f, 10f);
                Full(state);
            }

            public void Dispose()
            {
                hpObject.transform.SetParent(null);
                mpObject.transform.SetParent(null);
                UnityEngine.Object.DestroyImmediate(gaugeSprite);
                UnityEngine.Object.DestroyImmediate(hpObject);
                UnityEngine.Object.DestroyImmediate(mpObject);
                Invoke(fixture, "Dispose");
            }

            private static void Set(object target, string name, object value) =>
                target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);

            private static void Invoke(object target, string name, params object[] args) =>
                target.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic).Invoke(target, args);
        }
    }
}
