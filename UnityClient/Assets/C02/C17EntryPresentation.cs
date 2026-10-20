using C01;
using TMPro;
using UnityEngine;

namespace C02
{
    public sealed class C17EntryPresentation : MonoBehaviour
    {
        [SerializeField] private C01LoginScreen login;
        [SerializeField] private C02WorldScreen world;
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

            C02Stage stage = world.Stage;
            bool waitingForEntry = stage == C02Stage.Entering || stage == C02Stage.WaitingForZone ||
                stage == C02Stage.WaitingForFull;
            bool inField = stage == C02Stage.InWorld || stage == C02Stage.TransitionPending ||
                stage == C02Stage.ReconnectRequired;
            bool characterListReady = login.Authenticated && login.ListResponseCount > 0 && !login.IsBusy;
            bool showResults = !inField && !waitingForEntry && characterListReady && stage == C02Stage.Choosing;
            bool waitingForLoginResponse = login.IsBusy && (!login.Authenticated || login.ListResponseCount == 0);
            bool showSignIn = !inField && !waitingForEntry && (!showResults || waitingForLoginResponse);
            bool showLoading = waitingForEntry;

            signIn.SetActive(showSignIn);
            results.SetActive(showResults);
            loadingPanel.SetActive(showLoading);
            loadingText.text = stage switch
            {
                C02Stage.Entering => "Entering the world…",
                C02Stage.WaitingForZone => "Opening the field…",
                C02Stage.WaitingForFull => "Preparing your character…",
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
                returnLabel.text = showLoading ? "Cancel entry and change account" : "Change account";
        }

        private void ReturnToSignIn()
        {
            if (login == null) return;
            login.ConnectionFailed(world != null &&
                (world.Stage == C02Stage.Entering || world.Stage == C02Stage.WaitingForZone || world.Stage == C02Stage.WaitingForFull)
                ? "World entry cancelled."
                : "Session closed.");
        }

        private void OnDisable()
        {
            if (returnButton != null) returnButton.onClick.RemoveListener(ReturnToSignIn);
        }
    }
}
