using System;
using System.Reflection;
using Mmorpg.Client;
using NUnit.Framework;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace Mmorpg.Client.Tests
{
    public sealed class EntryPresentationTests
    {
        [Test]
        public void EntryStagesShowOnlyTheRelevantScreenAndPreserveTheField()
        {
            var root = new GameObject("Entry presentation fixture");
            root.SetActive(false);
            try
            {
                var login = root.AddComponent<LoginScreen>(); login.enabled = false;
                var world = root.AddComponent<WorldScreen>(); world.enabled = false;
                var screen = root.AddComponent<EntryPresentation>();
                GameObject Child(string name, params Type[] types)
                {
                    var child = new GameObject(name, types); child.transform.SetParent(root.transform); return child;
                }
                var signIn = Child("SignIn"); var results = Child("Results"); var loading = Child("Loading");
                var text = Child("LoadingText", typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TMP_Text>();
                var button = Child("Return", typeof(RectTransform), typeof(Button)).GetComponent<Button>();
                var buttonLabel = Child("ReturnLabel", typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TMP_Text>();
                buttonLabel.transform.SetParent(button.transform, false);
                void Bind(string name, object value) => typeof(EntryPresentation).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(screen, value);
                Bind("login", login); Bind("world", world); Bind("signIn", signIn); Bind("results", results);
                Bind("loadingPanel", loading); Bind("loadingText", text); Bind("returnButton", button);
                void Property(object target, string name, object value) => target.GetType().GetProperty(name).GetSetMethod(true).Invoke(target, new[] { value });
                void Stage(WorldStage stage)
                {
                    Property(world, "Stage", stage);
                    typeof(EntryPresentation).GetMethod("Refresh", BindingFlags.Instance | BindingFlags.NonPublic).Invoke(screen, null);
                }
                void LoginField(string name, object value) => typeof(LoginScreen).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(login, value);
                LoginField("loginIdInput", Child("Account", typeof(RectTransform), typeof(TMP_InputField)).GetComponent<TMP_InputField>());
                LoginField("passwordInput", Child("Password", typeof(RectTransform), typeof(TMP_InputField)).GetComponent<TMP_InputField>());
                LoginField("loginButton", Child("SignInButton", typeof(RectTransform), typeof(Button)).GetComponent<Button>());
                var status = Child("Status", typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TMP_Text>();
                LoginField("statusText", status);
                LoginField("characterListText", Child("List", typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TMP_Text>());
                root.SetActive(true); // Bind the actual return-button listener without starting a network client.
                Stage(WorldStage.Ready);
                Assert.That(signIn.activeSelf, Is.True); Assert.That(results.activeSelf || loading.activeSelf, Is.False);
                Property(login, "Authenticated", true); Property(login, "ListResponseCount", 1);
                Stage(WorldStage.Choosing);
                Assert.That(results.activeSelf, Is.True); Assert.That(signIn.activeSelf || loading.activeSelf, Is.False);
                Assert.That(button.transform.parent, Is.EqualTo(results.transform));
                foreach (var stage in new[] { WorldStage.Entering, WorldStage.WaitingForZone, WorldStage.WaitingForFull })
                {
                    Stage(stage);
                    Assert.That(loading.activeSelf, Is.True); Assert.That(signIn.activeSelf || results.activeSelf, Is.False);
                    Assert.That(text.text, Is.Not.Empty); Assert.That(text.text, Does.Not.Contain("%"));
                    Assert.That(button.transform.parent, Is.EqualTo(loading.transform));
                }
                foreach (var stage in new[] { WorldStage.InWorld, WorldStage.TransitionPending })
                {
                    Stage(stage); Assert.That(signIn.activeSelf || results.activeSelf || loading.activeSelf, Is.False);
                }
                Stage(WorldStage.ReconnectRequired);
                Assert.That(loading.activeSelf, Is.True);
                Assert.That(signIn.activeSelf || results.activeSelf, Is.False);
                Assert.That(button.gameObject.activeInHierarchy, Is.True);
                Assert.That(button.transform.parent, Is.EqualTo(loading.transform));
                Assert.That(buttonLabel.text, Is.EqualTo("Return to sign-in"));
                Assert.That(text.text, Does.Contain("reconnect"));
                Assert.That(login.Authenticated, Is.True, "The recovery screen must wait for the user's return action.");
                button.onClick.Invoke();
                Assert.That(login.Authenticated, Is.False);
                Assert.That(status.text, Does.Contain("Session closed"));
                Stage(WorldStage.Ready);
                Assert.That(signIn.activeSelf, Is.True);
                Assert.That(loading.activeSelf, Is.False);

                Property(login, "Authenticated", true);
                Stage(WorldStage.Entering);
                button.onClick.Invoke();
                Assert.That(login.Authenticated, Is.False);
                Assert.That(status.text, Does.Contain("cancelled"));
                Stage(WorldStage.Failed);
                Assert.That(signIn.activeSelf, Is.True); Assert.That(results.activeSelf || loading.activeSelf, Is.False);
            }
            finally { UnityEngine.Object.DestroyImmediate(root); }
        }
    }
}
