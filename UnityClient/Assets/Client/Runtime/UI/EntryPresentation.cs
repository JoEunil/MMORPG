using Mmorpg.Client;
using TMPro;
using UnityEngine;

namespace Mmorpg.Client
{
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C17EntryPresentation")]
    public sealed class EntryPresentation : MonoBehaviour
    {
        [SerializeField] private LoginScreen login;
        [SerializeField] private WorldScreen world;
        [SerializeField] private GameObject signIn;
        [SerializeField] private GameObject results;
        [SerializeField] private GameObject loadingPanel;
        [SerializeField] private TMP_Text loadingText;
        [SerializeField] private UnityEngine.UI.Button returnButton;
        private TMP_Text returnLabel;

        private void OnEnable()
        {
            if (returnButton != null)
            {
                returnButton.onClick.AddListener(ReturnToSignIn);
                returnLabel = returnButton.GetComponentInChildren<TMP_Text>(true);
            }
            Refresh();
        }

        private void Update() => Refresh();

        private void Refresh()
        {
            if (login == null || world == null || signIn == null || results == null ||
                loadingPanel == null || loadingText == null || returnButton == null) return;

            WorldStage stage = world.Stage;
            bool waitingForEntry = stage == WorldStage.Entering || stage == WorldStage.WaitingForZone ||
                stage == WorldStage.WaitingForFull;
            bool inField = stage == WorldStage.InWorld || stage == WorldStage.TransitionPending ||
                stage == WorldStage.ReconnectRequired;
            bool characterListReady = login.Authenticated && login.ListResponseCount > 0 && !login.IsBusy;
            bool showResults = !inField && !waitingForEntry && characterListReady && stage == WorldStage.Choosing;
            bool waitingForLoginResponse = login.IsBusy && (!login.Authenticated || login.ListResponseCount == 0);
            bool showSignIn = !inField && !waitingForEntry && (!showResults || waitingForLoginResponse);
            bool showLoading = waitingForEntry || stage == WorldStage.ReconnectRequired;

            signIn.SetActive(showSignIn);
            results.SetActive(showResults);
            loadingPanel.SetActive(showLoading);
            loadingText.text = stage switch
            {
                WorldStage.Entering => "Entering the world…",
                WorldStage.WaitingForZone => "Opening the field…",
                WorldStage.WaitingForFull => "Preparing your character…",
                WorldStage.ReconnectRequired => "The field could not be synchronized.\nReturn to sign-in to reconnect safely.",
                _ => string.Empty,
            };

            bool showReturn = showResults || showLoading;
            returnButton.gameObject.SetActive(showReturn);
            if (!showReturn) return;

            Transform target = showLoading ? loadingPanel.transform : results.transform;
            if (returnButton.transform.parent != target)
            {
                returnButton.transform.SetParent(target, false);
                returnButton.transform.SetAsLastSibling();
            }
            if (returnLabel != null)
                returnLabel.text = stage == WorldStage.ReconnectRequired ? "Return to sign-in" :
                    showLoading ? "Cancel entry and change account" : "Change account";
        }

        private void ReturnToSignIn()
        {
            if (login == null) return;
            login.ConnectionFailed(world != null &&
                (world.Stage == WorldStage.Entering || world.Stage == WorldStage.WaitingForZone || world.Stage == WorldStage.WaitingForFull)
                ? "World entry cancelled."
                : "Session closed.");
        }

        private void OnDisable()
        {
            if (returnButton != null) returnButton.onClick.RemoveListener(ReturnToSignIn);
        }
    }
}
