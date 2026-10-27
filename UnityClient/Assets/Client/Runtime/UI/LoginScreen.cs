using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using ClientCore;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace Mmorpg.Client
{
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C01", sourceAssembly: "C01.Runtime", sourceClassName: "C01LoginScreen")]
    public sealed class LoginScreen : MonoBehaviour, IMainThreadDispatcher, ClientCore.ILogger
    {
        [SerializeField] private TMP_InputField loginIdInput;
        [SerializeField] private TMP_InputField passwordInput;
        [SerializeField] private Button loginButton;
        [SerializeField] private TMP_Text statusText;
        [SerializeField] private TMP_Text characterListText;
        private readonly ConcurrentQueue<Action> jobs = new ConcurrentQueue<Action>();
        private Initializer client;
        private float deadline;
        public bool IsBusy { get; private set; }
        public bool Authenticated { get; private set; }
        public int ListResponseCount { get; private set; }
        public IViewModelUI Model => client?.GetViewModel();
        public IViewDataUI Data => client?.GetViewData();
        public event Action<IViewModelUI> ClientChanged;
        public event Action<byte, ushort> CharacterListReceived;
        public (ushort, List<CharacterInfoView>) Characters => client == null
            ? ((ushort)0, new List<CharacterInfoView>()) : client.GetViewData().GetCharList();

        private void OnEnable()
        {
            loginButton.onClick.AddListener(Submit);
            passwordInput.onSubmit.AddListener(SubmitPassword);
            Authenticated = false;
            ListResponseCount = 0;
            SetBusy(false);
            statusText.text = "Ready to sign in.";
            characterListText.text = "Sign in to request your characters.";
        }

        private void SubmitPassword(string _) => Submit();

        private async void Submit()
        {
            if (IsBusy) return;
            if (string.IsNullOrWhiteSpace(loginIdInput.text) || string.IsNullOrEmpty(passwordInput.text))
            {
                statusText.text = "Enter your login ID and password.";
                return;
            }
            ReleaseClient();
            client = new Initializer();
            client.Initialize(this, this);
            var model = client.GetViewModel();
            model.OnAuthReceived += AuthReceived;
            model.OnCharacterListResult += ListReceived;
            model.OnConnectionFailed += ConnectionFailed;
            ClientChanged?.Invoke(model);
            Authenticated = false;
            characterListText.text = "Waiting for the server response...";
            statusText.text = "Signing in...";
            SetBusy(true);
            string password = passwordInput.text;
            passwordInput.text = string.Empty;
            await model.Login(loginIdInput.text.Trim(), password);
        }

        private void AuthReceived(bool success)
        {
            Authenticated = success;
            statusText.text = success ? "Authenticated. Requesting characters..." : "Game server authentication failed.";
        }

        private void ListReceived(byte status, ushort count)
        {
            ListResponseCount++;
            SetBusy(false);
            if (status == 0)
            {
                statusText.text = $"No characters or list request failed (status={status}, count={count})";
                characterListText.text = "No character rows returned.\nThe server does not distinguish an empty list from a failed query.";
                CharacterListReceived?.Invoke(status, count);
                return;
            }
            statusText.text = $"Character list received (status={status}, count={count})";
            var rows = new StringBuilder();
            foreach (var character in Characters.Item2)
                rows.AppendLine($"{character.Name}   /   Level {character.Level}   /   ID {character.CharacterID}");
            characterListText.text = count == 0 ? "Your character list is empty." : rows.ToString();
            CharacterListReceived?.Invoke(status, count);
        }

        public void ConnectionFailed(string message)
        {
            SetBusy(false);
            Authenticated = false;
            statusText.text = message + " Retry when ready.";
            characterListText.text = "No character list available.";
            ReleaseClient();
        }

        public void SetBusy(bool busy)
        {
            IsBusy = busy;
            if (busy) deadline = Time.unscaledTime + 15f;
            loginButton.interactable = !busy;
            loginIdInput.interactable = !busy;
            passwordInput.interactable = !busy;
        }

        private void Update()
        {
            while (jobs.TryDequeue(out var job)) job();
            if (IsBusy && Time.unscaledTime >= deadline)
                ConnectionFailed("The server response timed out.");
        }

        public void Post(Action job) => jobs.Enqueue(job);
        public void Log(string message) => Post(() => Debug.Log(message));

        public void ReleaseClient()
        {
            if (client == null) return;
            ClientChanged?.Invoke(null);
            var model = client.GetViewModel();
            model.OnAuthReceived -= AuthReceived;
            model.OnCharacterListResult -= ListReceived;
            model.OnConnectionFailed -= ConnectionFailed;
            client.Dispose();
            client = null;
        }

        private void OnDisable()
        {
            loginButton.onClick.RemoveListener(Submit);
            passwordInput.onSubmit.RemoveListener(SubmitPassword);
            ReleaseClient();
            passwordInput.text = string.Empty;
            IsBusy = false;
            while (jobs.TryDequeue(out _)) { }
        }
    }
}
