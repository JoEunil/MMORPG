using ClientCore.Network;
using ClientCore.PacketHelper;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Drawing;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using System.Xml.Linq;


namespace ClientCore
{
    public class ViewModel : IViewModelUI, IViewModelNet, IViewModelTick, IDisposable
    {
        private readonly INetworkService _network;
        private readonly IViewDataModel _viewData;
        private readonly ILogger _logger;
        private readonly IMainThreadDispatcher _threadDispatcher;
        private readonly CancellationTokenSource _lifetime = new CancellationTokenSource();
        private readonly object _sessionGate = new object();
        private bool _enterPending;
        private bool _zonePending;
        private volatile bool _connectionFailed;
        // Profile state belongs to the existing main-thread dispatcher, never the tick thread.
        private readonly Dictionary<uint, ProfileEntry> _profiles = new Dictionary<uint, ProfileEntry>();
        private readonly Dictionary<uint, uint> _profileVersions = new Dictionary<uint, uint>();
        private readonly Dictionary<uint, uint> _entityProfiles = new Dictionary<uint, uint>();
        private readonly Dictionary<uint, uint> _profilesPending = new Dictionary<uint, uint>();
        private readonly Dictionary<uint, uint> _profilesMissing = new Dictionary<uint, uint>();
        private long _profileRequestDeadline;
        private bool _worldReady;
        private bool _inventoryPending;
        public event Action<bool> OnAuthReceived;
        public event Action<byte, ushort> OnCharacterListResult;
        public event Action<string> OnConnectionFailed;

        public event Action<bool> OnEnterSuccess;
        public event Action<bool> OnCharacterListReceived;
        public event Action<byte, ulong, string, string> OnChatReceived;
        public event Action<bool> OnInventoryReceived;
        public event Action<ushort, ulong, uint, float, float> OnZoneChageReceived;
        public event Action<ushort, DeltaUpdateField[]> OnDeltaReceived;
        public event Action<ushort, FullStateField[]> OnFullReceived;
        public event Action<ushort, MonsterDeltaField[]> OnMonsterDeltaReceived;
        public event Action<ushort, MonsterFullField[]> OnMonsterFullReceived;
        public event Action<ushort, ActionResultField[]> OnActionResultReceived;
        public event Action OnZoneChageFailed;
        public event Action<ulong> OnPingReceived;

        internal ViewModel(INetworkService network, IViewDataModel viewData, ILogger logger, IMainThreadDispatcher threadDispatcher)
        {
            _network = network;
            _viewData = viewData;
            _logger = logger;
            _threadDispatcher = threadDispatcher;
        }
        public async Task Connect()
        {
            var session = await AuthService.Instance.GetSessionAsync(ClientSession.AccessToken, _lifetime.Token).ConfigureAwait(false);
            lock (_sessionGate)
            {
                _lifetime.Token.ThrowIfCancellationRequested();
                ClientSession.SessionToken = session.SessionToken;
            }
            await _network.Connect(session.GameServer.IP, session.GameServer.Port).ConfigureAwait(false);
        }

        // UI
        public async Task Login(string id, string pwd)
        {
            try
            {
                _lifetime.Token.ThrowIfCancellationRequested();
                _viewData.SetCharList(0, Array.Empty<PacketHelper.CharacterInfo>());
                var login = await AuthService.Instance.LoginAsync(id, pwd, _lifetime.Token).ConfigureAwait(false);
                lock (_sessionGate)
                {
                    _lifetime.Token.ThrowIfCancellationRequested();
                    ClientSession.AccessToken = login.AccessToken;
                    ClientSession.UserID = login.UserID;
                }
                await Connect().ConfigureAwait(false);
            }
            catch (OperationCanceledException) when (_lifetime.IsCancellationRequested) { }
            catch (Exception ex)
            {
                Error(ex is InvalidOperationException ? ex.Message : "Login or connection failed.");
            }
        }

        public void Dispose()
        {
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested) return;
                _lifetime.Cancel();
                _inventoryPending = false;
                _worldReady = false;
                _viewData.SetInventory(Array.Empty<PacketHelper.InventoryItem>());
                ClientSession.AccessToken = null;
                ClientSession.SessionToken = null;
                ClientSession.UserID = 0;
            }
        }
        public void Enter(ulong charID)
        {
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || _enterPending || _zonePending) return;
                _enterPending = true;
            }
            try { _network.Enter(charID); }
            catch (Exception)
            {
                lock (_sessionGate) _enterPending = false;
                Error("World entry request failed.");
            }
        }

        public void Chat(string message, byte scope, ulong targetID)
        {
            if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady)
                throw new InvalidOperationException("Enter the world before chatting.");
            _network.Chat(message, scope, targetID);
        }
        public void ZoneChange(int op)
        {
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || _zonePending) return;
                _zonePending = true;
            }
            try { _network.ZoneChange((byte)op); }
            catch (Exception)
            {
                lock (_sessionGate) _zonePending = false;
                Error("Zone entry request failed.");
            }
        }

        // Client Tick 루프
        public void SendUpdate()
        {
            (bool, byte, float, float, byte) res;
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed) return;
                res = _viewData.GetActionState();
            }
            // Do not hold the session lock across socket I/O: receive callbacks also use it.
            if (!res.Item1 || _lifetime.IsCancellationRequested || _connectionFailed) return;
            try { _network.Action(res.Item2, res.Item3, res.Item4, res.Item5); }
            catch (Exception) { Error("Movement request failed."); }
        }

        // Network
        // Post 인자로 넘기는 lambda 함수 자체가 action으로 처리됨. 람다 함수가 action으로 암시적 형변환이 일어남.
        public void AuthReceived(byte resStatus)
        {
            if (_lifetime.IsCancellationRequested) return;
            bool success = resStatus != 0;
            _threadDispatcher.Post(() => { if (!_lifetime.IsCancellationRequested) OnAuthReceived?.Invoke(success); });
            if (success)
                _network.CharacterList();
            else
                Error("Game server authentication failed.");
        }

        public void ChatacterListReceived(byte resStatus, ushort count, PacketHelper.CharacterInfo[] characters)
        {
            bool success = resStatus != 0;
            if (_lifetime.IsCancellationRequested) return;
            _viewData.SetCharList(success ? count : (ushort)0, success ? characters : Array.Empty<PacketHelper.CharacterInfo>());
            _threadDispatcher.Post(() =>
            {
                if (_lifetime.IsCancellationRequested) return;
                OnCharacterListResult?.Invoke(resStatus, count);
                OnCharacterListReceived?.Invoke(success);
            });
        }

        public void EnterReceived(byte resStatus, byte[] name, ushort attack, ushort level, uint exp, int hp, int mp, int maxHP, int maxMP, byte dir, float startX, float startY, ushort currentZone)
        {
            bool success = resStatus != 0;
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || !_enterPending) return;
                _enterPending = false;
                _threadDispatcher.Post(() =>
                {
                    if (!_lifetime.IsCancellationRequested) OnEnterSuccess?.Invoke(success);
                });
                if (success) _viewData.SetPosition(startX, startY);
            }
            if (success) ZoneChange((byte)ZONE_CHANGE.ENTER);
        }

        public void ChatReceived(Packet.Message message)
        {
            _threadDispatcher.Post(() =>
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed) return;
                ObserveProfile(message.senderProfileId, 0);
                string name = _profiles.TryGetValue(message.senderProfileId, out var profile) && !string.IsNullOrEmpty(profile.name)
                    ? profile.name : "플레이어 #" + message.senderID;
                OnChatReceived?.Invoke(message.scope, message.senderID, name, message.message);
                RequestProfiles();
            });
        }

        private void ObserveProfile(uint id, uint version)
        {
            if (id != 0 && (!_profileVersions.TryGetValue(id, out var current) || version > current))
                _profileVersions[id] = version;
        }

        private void RequestProfiles()
        {
            // There is no response request ID, so keep a single batch in flight.
            if (_connectionFailed || _lifetime.IsCancellationRequested) return;
            if (_profilesPending.Count != 0)
            {
                if (Stopwatch.GetTimestamp() < _profileRequestDeadline) return;
                foreach (var pair in _profilesPending) _profilesMissing[pair.Key] = pair.Value;
                _profilesPending.Clear();
            }
            foreach (var pair in _profileVersions)
            {
                if (_profiles.TryGetValue(pair.Key, out var cached) && cached.version >= pair.Value ||
                    _profilesMissing.TryGetValue(pair.Key, out var missing) && missing >= pair.Value) continue;
                _profilesPending.Add(pair.Key, pair.Value);
                if (_profilesPending.Count == 50) break;
            }
            if (_profilesPending.Count == 0) return;
            _profileRequestDeadline = Stopwatch.GetTimestamp() + 5 * Stopwatch.Frequency;
            try { _network.Profiles(_profilesPending.Keys.ToArray()); }
            catch (Exception)
            {
                foreach (var pair in _profilesPending) _profilesMissing[pair.Key] = pair.Value;
                _profilesPending.Clear();
                _logger.Log("Profile lookup request failed.");
            }
        }

        public void ProfilesReceived(ProfileEntry[] profiles)
        {
            _threadDispatcher.Post(() =>
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed) return;
                foreach (var profile in profiles)
                {
                    if (!_profilesPending.TryGetValue(profile.profileId, out var requested) || profile.version < requested) continue;
                    if (_profileVersions.TryGetValue(profile.profileId, out var observed) && profile.version < observed) continue;
                    if (!_profiles.TryGetValue(profile.profileId, out var cached) || profile.version >= cached.version)
                        _profiles[profile.profileId] = profile;
                    _profilesPending.Remove(profile.profileId);
                }
                // Omitted IDs cannot acknowledge a batch: an empty reply may belong to a timed-out request.
                // They remain pending until the existing dispatcher traffic observes its deadline.
                RequestProfiles();
            });
        }

        public void RequestInventory()
        {
            if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady || _inventoryPending)
                throw new InvalidOperationException("Inventory is unavailable or a request is pending.");
            _inventoryPending = true;
            _viewData.SetInventory(Array.Empty<PacketHelper.InventoryItem>());
            try { _network.RequestInventory(); }
            catch { _inventoryPending = false; throw; }
        }

        public void InventoryReceived(byte resStatus, PacketHelper.InventoryItem[] items)
        {
            _threadDispatcher.Post(() =>
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady || !_inventoryPending) return;
                _inventoryPending = false;
                bool success = resStatus == 1;
                _viewData.SetInventory(success ? items : Array.Empty<PacketHelper.InventoryItem>());
                OnInventoryReceived?.Invoke(success);
            });
        }

        public void ZoneChageReceived(byte resStatus, ushort zoneID, ulong chatID, uint zoneInternalID, float x, float y)
        {
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || !_zonePending) return;
                _zonePending = false;
                bool success = resStatus != 0;
                if (success) _viewData.SetPosition(x, y);
                _threadDispatcher.Post(() =>
                {
                    if (_lifetime.IsCancellationRequested) return;
                    _worldReady = success;
                    _entityProfiles.Clear();
                    if (success) OnZoneChageReceived?.Invoke(zoneID, chatID, zoneInternalID, x, y);
                    else OnZoneChageFailed?.Invoke();
                });
            }
        }

        public void DeltaReceived(ushort count, PacketHelper.DeltaUpdateField[] updates)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnDeltaReceived;
            _threadDispatcher.Post(() =>
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed) return;
                for (int i = 0; i < count; i++)
                    if (updates[i].fieldID == 9 && _entityProfiles.TryGetValue(updates[i].zoneInternalID, out var id))
                        ObserveProfile(id, updates[i].fieldVal);
                RequestProfiles();
                subscribers?.Invoke(count, updates);
            });
        }
        public void FullReceived(ushort count, PacketHelper.FullStateField[] states)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnFullReceived;
            _threadDispatcher.Post(() =>
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed) return;
                for (int i = 0; i < count; i++)
                {
                    _entityProfiles[states[i].zoneInternalID] = states[i].profileId;
                    ObserveProfile(states[i].profileId, states[i].profileVersion);
                }
                RequestProfiles();
                subscribers?.Invoke(count, states);
            });
        }
       public void MonsterDeltaReceived(ushort count, PacketHelper.MonsterDeltaField[] updates)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnMonsterDeltaReceived;
            _threadDispatcher.Post(() =>
            {
                if (!_lifetime.IsCancellationRequested) subscribers?.Invoke(count, updates);
            });
        }
        public void MonsterFullReceived(ushort count, PacketHelper.MonsterFullField[] states)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnMonsterFullReceived;
            _threadDispatcher.Post(() =>
            {
                if (!_lifetime.IsCancellationRequested) subscribers?.Invoke(count, states);
            });
        }
        public void ActionResultReceived(ushort count, PacketHelper.ActionResultField[] actions)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnActionResultReceived;
            _threadDispatcher.Post(() =>
            {
                if (!_lifetime.IsCancellationRequested) subscribers?.Invoke(count, actions);
            });
        }
        public void PingReceived(ulong servertimeMs, ulong rtt)
        {
            _threadDispatcher.Post(() =>
            {
                OnPingReceived?.Invoke(rtt);
            });
            _network.Pong(servertimeMs);
        }

        public void Error(string msg)
        {
            if (_lifetime.IsCancellationRequested) return;
            _connectionFailed = true;
            _worldReady = false;
            _inventoryPending = false;
            _viewData.SetInventory(Array.Empty<PacketHelper.InventoryItem>());
            _viewData.SetCharList(0, Array.Empty<PacketHelper.CharacterInfo>());
            _threadDispatcher.Post(() =>
            {
                _profiles.Clear();
                _profileVersions.Clear();
                _entityProfiles.Clear();
                _profilesPending.Clear();
                _profilesMissing.Clear();
                if (!_lifetime.IsCancellationRequested) OnConnectionFailed?.Invoke(msg);
            });
        }
        public void Log(string msg)
        {
            _logger.Log(msg);
        }
    }
}
