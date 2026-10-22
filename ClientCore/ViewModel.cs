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
        private bool _respawnPending;
        private long _respawnRetryDeadline;
        private int _selfHP;
        private volatile bool _connectionFailed;
        // Profile state belongs to the existing main-thread dispatcher, never the tick thread.
        private readonly Dictionary<uint, ProfileEntry> _profiles = new Dictionary<uint, ProfileEntry>();
        private readonly Dictionary<uint, uint> _profileVersions = new Dictionary<uint, uint>();
        private readonly Dictionary<uint, uint> _entityProfiles = new Dictionary<uint, uint>();
        private readonly Dictionary<uint, uint> _profilesPending = new Dictionary<uint, uint>();
        private readonly Dictionary<uint, uint> _profilesMissing = new Dictionary<uint, uint>();
        private long _profileRequestDeadline;
        private volatile bool _worldReady;
        private int _zoneOperation;
        // Only the dispatcher reads or writes the accepted simulation identity.
        private ushort _zoneID;
        private ulong _zoneGeneration;
        private ulong _ownershipEpoch;
        private uint _zoneInternalID;
        private ulong _pendingOwnershipEpoch;
        private ulong _pendingBootstrapID;
        private ulong _pendingChatID;
        private uint _pendingZoneInternalID;
        private ushort _pendingZoneID;
        private float _pendingStartX, _pendingStartY;
        private ulong _pendingGeneration;
        private long _bootstrapDeadline;
        private bool _playerBootstrapEnd;
        private bool _monsterBootstrapEnd;
        private PacketHelper.FullStateField[] _bootstrapPlayers;
        private PacketHelper.MonsterFullField[] _bootstrapMonsters;
        private volatile bool _awaitingZoneAck;
        private bool _inventoryPending;
        private uint _bazaarRequestID;
        private uint _bazaarPending;
        private enum BazaarRequestKind { None, Search, MyList, Claim, Buy, CheckOutbox, CancelListing }
        private BazaarRequestKind _bazaarPendingKind;
        private ulong _bazaarPendingListingID;
        public event Action<bool> OnAuthReceived;
        public event Action<byte, ushort> OnCharacterListResult;
        public event Action<string> OnConnectionFailed;

        public event Action<bool> OnEnterSuccess;
        public event Action<bool> OnCharacterListReceived;
        public event Action<byte, ulong, string, string> OnChatReceived;
        public event Action<bool> OnInventoryReceived;
        public event Action<bool> OnBazaarReceived;
        public event Action<ulong, byte, ulong> OnBazaarClaimReceived;
        public event Action<ulong, byte, uint, uint, ulong> OnBazaarBuyReceived;
        public event Action<byte, uint, uint, uint> OnBazaarCheckOutboxReceived;
        public event Action<ulong, byte> OnBazaarCancelListingReceived;
        public event Action<ushort, ulong, uint, float, float> OnZoneChageReceived;
        public event Action<ulong, ushort> OnZoneTransitionStarted;
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
                _respawnPending = false;
                _zonePending = false;
                CancelBazaar();
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
                if (op != (int)ZONE_CHANGE.ENTER) throw new ArgumentOutOfRangeException(nameof(op));
                if (_lifetime.IsCancellationRequested || _connectionFailed || _zonePending) return;
                if (op != 0 && !_worldReady) return;
                if (op == 0 && _zoneID != 0) return;
                _zoneOperation = op;
                _zonePending = true;
                CancelBazaar();
                _awaitingZoneAck = true;
                _worldReady = false;
            }
            try { _network.ZoneChange((byte)op); }
            catch (Exception)
            {
                lock (_sessionGate) _zonePending = false;
                Error("Zone entry request failed.");
            }
        }

        public bool RequestRespawn()
        {
            ulong epoch;
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed || _ownershipEpoch == 0)
                    return false;
                if (_respawnPending)
                {
                    if (!_zonePending || Stopwatch.GetTimestamp() < _respawnRetryDeadline)
                        return false;
                    _respawnRetryDeadline = Stopwatch.GetTimestamp() + 8 * Stopwatch.Frequency;
                }
                else
                {
                    if (!_worldReady || _zonePending || _selfHP != 0)
                        return false;
                    _respawnPending = true;
                    _zonePending = true;
                    _awaitingZoneAck = true;
                    _worldReady = false;
                    _respawnRetryDeadline = Stopwatch.GetTimestamp() + 8 * Stopwatch.Frequency;
                    _viewData.CancelAction();
                }
                epoch = _ownershipEpoch;
            }
            try
            {
                _network.RespawnToTown(epoch);
                return true;
            }
            catch (Exception)
            {
                _logger.Log("Respawn request could not be sent.");
                return false;
            }
        }

        // Client Tick 루프
        public void SendUpdate()
        {
            (bool, byte, float, float, byte, ulong) res;
            ulong resyncEpoch = 0;
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed) return;
                if (!_worldReady)
                {
                    if (_pendingOwnershipEpoch != 0 && Stopwatch.GetTimestamp() >= _bootstrapDeadline)
                    {
                        resyncEpoch = _pendingOwnershipEpoch;
                        _bootstrapDeadline = Stopwatch.GetTimestamp() + 2 * Stopwatch.Frequency;
                    }
                    else return;
                    res = default;
                }
                else
                    res = _viewData.GetActionState();
            }
            if (resyncEpoch != 0)
            {
                try { _network.ZoneResync(resyncEpoch); }
                catch (Exception) { Error("Zone baseline resync request failed."); }
                return;
            }
            // Do not hold the session lock across socket I/O: receive callbacks also use it.
            if (!res.Item1 || _lifetime.IsCancellationRequested || _connectionFailed || !_worldReady) return;
            if (res.Item6 == 0) return;
            try { _network.Action(res.Item2, res.Item3, res.Item4, res.Item5, res.Item6); }
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
                _selfHP = hp;
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

        public void RequestBazaar(ushort itemType, ushort page)
        {
            if (itemType < 1 || itemType > 3) throw new ArgumentOutOfRangeException(nameof(itemType));
            uint requestID = BeginBazaarRequest(BazaarRequestKind.Search);
            try { _network.RequestBazaar(requestID, itemType, page); }
            catch
            {
                lock (_sessionGate) if (_bazaarPending == requestID) CancelBazaar();
                throw;
            }
        }

        public void RequestBazaarMyList()
        {
            uint requestID = BeginBazaarRequest(BazaarRequestKind.MyList);
            try { _network.RequestBazaarMyList(requestID); }
            catch
            {
                lock (_sessionGate) if (_bazaarPending == requestID) CancelBazaar();
                throw;
            }
        }

        public void RequestBazaarClaim(ulong listingID)
        {
            if (listingID == 0) throw new ArgumentOutOfRangeException(nameof(listingID));
            uint requestID = BeginBazaarRequest(BazaarRequestKind.Claim, listingID);
            try { _network.RequestBazaarClaim(requestID, listingID); }
            catch
            {
                lock (_sessionGate) if (_bazaarPending == requestID) CancelBazaar();
                throw;
            }
        }

        public void RequestBazaarBuy(ulong listingID)
        {
            if (listingID == 0) throw new ArgumentOutOfRangeException(nameof(listingID));
            uint requestID = BeginBazaarRequest(BazaarRequestKind.Buy, listingID);
            try { _network.RequestBazaarBuy(requestID, listingID); }
            catch
            {
                lock (_sessionGate) if (_bazaarPending == requestID) CancelBazaar();
                throw;
            }
        }

        public void RequestBazaarCheckOutbox()
        {
            uint requestID = BeginBazaarRequest(BazaarRequestKind.CheckOutbox);
            try { _network.RequestBazaarCheckOutbox(requestID); }
            catch
            {
                lock (_sessionGate) if (_bazaarPending == requestID) CancelBazaar();
                throw;
            }
        }

        public void RequestBazaarCancelListing(ulong listingID)
        {
            if (listingID == 0) throw new ArgumentOutOfRangeException(nameof(listingID));
            uint requestID = BeginBazaarRequest(BazaarRequestKind.CancelListing, listingID);
            try { _network.RequestBazaarCancelListing(requestID, listingID); }
            catch
            {
                lock (_sessionGate) if (_bazaarPending == requestID) CancelBazaar();
                throw;
            }
        }

        private uint BeginBazaarRequest(BazaarRequestKind kind, ulong listingID = 0)
        {
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady || _bazaarPending != 0)
                    throw new InvalidOperationException("Bazaar is unavailable or pending.");
                if (_bazaarRequestID == uint.MaxValue) throw new InvalidOperationException("Reconnect before requesting bazaar again.");
                _bazaarPending = ++_bazaarRequestID;
                _bazaarPendingKind = kind;
                _bazaarPendingListingID = listingID;
                if (kind == BazaarRequestKind.Search || kind == BazaarRequestKind.MyList)
                    _viewData.SetBazaar(Array.Empty<PacketHelper.BazaarListing>());
                return _bazaarPending;
            }
        }

        public void CancelBazaar()
        {
            lock (_sessionGate)
            {
                _bazaarPending = 0;
                _bazaarPendingKind = BazaarRequestKind.None;
                _bazaarPendingListingID = 0;
                _viewData.SetBazaar(Array.Empty<PacketHelper.BazaarListing>());
            }
        }

        public void BazaarReceived(uint requestID, byte status, PacketHelper.BazaarListing[] rows)
        {
            ReceiveBazaar(requestID, status, rows, BazaarRequestKind.Search);
        }

        public void BazaarMyListReceived(uint requestID, byte status, PacketHelper.BazaarListing[] rows)
        {
            ReceiveBazaar(requestID, status, rows, BazaarRequestKind.MyList);
        }

        public void BazaarClaimReceived(uint requestID, ulong listingID, byte status, ulong diamondClaimed)
        {
            if (requestID == 0 || listingID == 0 || status > 4 || status != 1 && diamondClaimed != 0) return;
            _threadDispatcher.Post(() =>
            {
                lock (_sessionGate)
                {
                    if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady ||
                        requestID != _bazaarPending || _bazaarPendingKind != BazaarRequestKind.Claim ||
                        listingID != _bazaarPendingListingID) return;
                    _bazaarPending = 0;
                    _bazaarPendingKind = BazaarRequestKind.None;
                    _bazaarPendingListingID = 0;
                }
                OnBazaarClaimReceived?.Invoke(listingID, status, diamondClaimed);
            });
        }

        public void BazaarBuyReceived(uint requestID, ulong listingID, byte status, uint itemID, uint quantity, ulong diamondSpent)
        {
            if (requestID == 0 || listingID == 0 || (status != 0 && status != 1 && status != 3 && status != 4 && status != 5) ||
                (status == 1 ? itemID == 0 || quantity == 0 : itemID != 0 || quantity != 0 || diamondSpent != 0)) return;
            _threadDispatcher.Post(() =>
            {
                lock (_sessionGate)
                {
                    if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady ||
                        requestID != _bazaarPending || _bazaarPendingKind != BazaarRequestKind.Buy ||
                        listingID != _bazaarPendingListingID) return;
                    _bazaarPending = 0;
                    _bazaarPendingKind = BazaarRequestKind.None;
                    _bazaarPendingListingID = 0;
                }
                OnBazaarBuyReceived?.Invoke(listingID, status, itemID, quantity, diamondSpent);
            });
        }

        public void BazaarCheckOutboxReceived(uint requestID, byte status, uint delivered, uint duplicated, uint blocked)
        {
            if (requestID == 0 || status > 1 || (ulong)delivered + duplicated + blocked > 30) return;
            _threadDispatcher.Post(() =>
            {
                lock (_sessionGate)
                {
                    if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady ||
                        requestID != _bazaarPending || _bazaarPendingKind != BazaarRequestKind.CheckOutbox) return;
                    _bazaarPending = 0;
                    _bazaarPendingKind = BazaarRequestKind.None;
                    _bazaarPendingListingID = 0;
                }
                OnBazaarCheckOutboxReceived?.Invoke(status, delivered, duplicated, blocked);
            });
        }

        public void BazaarCancelListingReceived(uint requestID, ulong listingID, byte status)
        {
            if (requestID == 0 || listingID == 0 || (status != 0 && status != 1 && status != 5)) return;
            _threadDispatcher.Post(() =>
            {
                lock (_sessionGate)
                {
                    if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady ||
                        requestID != _bazaarPending || _bazaarPendingKind != BazaarRequestKind.CancelListing ||
                        listingID != _bazaarPendingListingID) return;
                    _bazaarPending = 0;
                    _bazaarPendingKind = BazaarRequestKind.None;
                    _bazaarPendingListingID = 0;
                }
                OnBazaarCancelListingReceived?.Invoke(listingID, status);
            });
        }

        private void ReceiveBazaar(uint requestID, byte status, PacketHelper.BazaarListing[] rows, BazaarRequestKind kind)
        {
            // Own the values crossing the receive-thread/dispatcher boundary.
            var snapshot = status == 1 ? (PacketHelper.BazaarListing[])rows.Clone() : Array.Empty<PacketHelper.BazaarListing>();
            _threadDispatcher.Post(() =>
            {
                lock (_sessionGate)
                {
                    if (_lifetime.IsCancellationRequested || _connectionFailed || !_worldReady || requestID == 0 ||
                        requestID != _bazaarPending || kind != _bazaarPendingKind) return;
                    _bazaarPending = 0;
                    _bazaarPendingKind = BazaarRequestKind.None;
                    _viewData.SetBazaar(snapshot);
                }
                OnBazaarReceived?.Invoke(status == 1);
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
                // This response still belongs to the same character during a zone transfer.
                if (_lifetime.IsCancellationRequested || _connectionFailed || !_inventoryPending) return;
                _inventoryPending = false;
                bool success = resStatus == 1;
                _viewData.SetInventory(success ? items : Array.Empty<PacketHelper.InventoryItem>());
                OnInventoryReceived?.Invoke(success);
            });
        }

        public void ZoneChageReceived(byte resStatus, ushort zoneID, ulong chatID, uint zoneInternalID, float x, float y, ulong generation, ulong ownershipEpoch)
        {
            lock (_sessionGate)
            {
                if (_lifetime.IsCancellationRequested) return;
                bool success = resStatus == 1;
                bool requestPending = _zonePending;
                bool automaticTransition = success && !requestPending && ownershipEpoch > _ownershipEpoch;
                if (!requestPending && !automaticTransition) return;
                if (success && ownershipEpoch <= _ownershipEpoch) return;
                if (requestPending) _zonePending = false;
                bool wasInWorld = _zoneID != 0;
                _threadDispatcher.Post(() =>
                {
                    if (_lifetime.IsCancellationRequested || _connectionFailed) return;
                    // Multiple repair ACKs can be queued before the dispatcher accepts the first.
                    // A duplicate success is never a failed transition.
                    if (success && ownershipEpoch <= _ownershipEpoch) return;
                    if (success && ownershipEpoch > _ownershipEpoch)
                    {
                        lock (_sessionGate)
                        {
                            _awaitingZoneAck = true;
                            _worldReady = false;
                            _ownershipEpoch = ownershipEpoch;
                            _bootstrapDeadline = Stopwatch.GetTimestamp() + 2 * Stopwatch.Frequency;
                            _pendingOwnershipEpoch = ownershipEpoch;
                            _pendingBootstrapID = 0;
                        }
                        _pendingZoneID = zoneID;
                        _pendingChatID = chatID;
                        _pendingZoneInternalID = zoneInternalID;
                        _pendingStartX = x;
                        _pendingStartY = y;
                        _pendingGeneration = generation;
                        _bootstrapPlayers = null;
                        _bootstrapMonsters = null;
                        _playerBootstrapEnd = false;
                        _monsterBootstrapEnd = false;
                        _viewData.CancelAction();
                        _viewData.SetPosition(x, y);
                        _viewData.SetOwnershipEpoch(ownershipEpoch);
                        if (wasInWorld)
                        {
                            CancelBazaar();
                            OnZoneTransitionStarted?.Invoke(ownershipEpoch, zoneID);
                        }
                    }
                    else
                    {
                        _worldReady = wasInWorld;
                        _awaitingZoneAck = false;
                        if (_respawnPending)
                        {
                            _respawnPending = false;
                            _respawnRetryDeadline = 0;
                        }
                        OnZoneChageFailed?.Invoke();
                    }
                });
            }
        }

        private bool AcceptSimulation(ushort zoneID, ulong generation) => !_lifetime.IsCancellationRequested &&
            !_connectionFailed && !_awaitingZoneAck && _zoneID != 0 && zoneID == _zoneID && generation >= _zoneGeneration;

        public void DeltaReceived(ushort zoneID, ulong generation, ushort count, PacketHelper.DeltaUpdateField[] updates)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnDeltaReceived;
            _threadDispatcher.Post(() =>
            {
                if (!AcceptSimulation(zoneID, generation) || !_worldReady) return;
                for (int i = 0; i < count; i++)
                {
                    if (updates[i].zoneInternalID == _zoneInternalID && updates[i].fieldID == 0)
                        _selfHP = unchecked((int)updates[i].fieldVal);
                    if (updates[i].fieldID == 9 && _entityProfiles.TryGetValue(updates[i].zoneInternalID, out var id))
                        ObserveProfile(id, updates[i].fieldVal);
                }
                RequestProfiles();
                subscribers?.Invoke(count, updates);
            });
        }
        public void FullReceived(ushort zoneID, ulong generation, ulong ownershipEpoch, ulong bootstrapID, bool bootstrapEnd, ushort count, PacketHelper.FullStateField[] states)
        {
            if (_lifetime.IsCancellationRequested) return;
            if (bootstrapID != 0)
            {
                BootstrapReceived(false, zoneID, generation, ownershipEpoch, bootstrapID, bootstrapEnd, states, null);
                return;
            }
            var subscribers = OnFullReceived;
            _threadDispatcher.Post(() =>
            {
                if (!AcceptSimulation(zoneID, generation) || !_worldReady) return;
                for (int i = 0; i < count; i++)
                {
                    if (states[i].zoneInternalID == _zoneInternalID)
                        _selfHP = states[i].hp;
                    _entityProfiles[states[i].zoneInternalID] = states[i].profileId;
                    ObserveProfile(states[i].profileId, states[i].profileVersion);
                }
                RequestProfiles();
                subscribers?.Invoke(count, states);
            });
        }
       public void MonsterDeltaReceived(ushort zoneID, ulong generation, ushort count, PacketHelper.MonsterDeltaField[] updates)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnMonsterDeltaReceived;
            _threadDispatcher.Post(() =>
            {
                if (AcceptSimulation(zoneID, generation) && _worldReady) subscribers?.Invoke(count, updates);
            });
        }
        public void MonsterFullReceived(ushort zoneID, ulong generation, ulong ownershipEpoch, ulong bootstrapID, bool bootstrapEnd, ushort count, PacketHelper.MonsterFullField[] states)
        {
            if (_lifetime.IsCancellationRequested) return;
            if (bootstrapID != 0)
            {
                BootstrapReceived(true, zoneID, generation, ownershipEpoch, bootstrapID, bootstrapEnd, null, states);
                return;
            }
            var subscribers = OnMonsterFullReceived;
            _threadDispatcher.Post(() =>
            {
                if (AcceptSimulation(zoneID, generation) && _worldReady) subscribers?.Invoke(count, states);
            });
        }

        private void BootstrapReceived(bool monsters, ushort zoneID, ulong generation, ulong ownershipEpoch,
            ulong bootstrapID, bool bootstrapEnd, PacketHelper.FullStateField[] players, PacketHelper.MonsterFullField[] monsterStates)
        {
            _threadDispatcher.Post(() =>
            {
                if (_lifetime.IsCancellationRequested || _connectionFailed || ownershipEpoch != _pendingOwnershipEpoch
                    || bootstrapID == 0 || zoneID != _pendingZoneID || generation < _pendingGeneration || !bootstrapEnd)
                    return;
                if (bootstrapID < _pendingBootstrapID) return;
                if (bootstrapID > _pendingBootstrapID)
                {
                    _pendingBootstrapID = bootstrapID;
                    _bootstrapPlayers = null;
                    _bootstrapMonsters = null;
                    _playerBootstrapEnd = false;
                    _monsterBootstrapEnd = false;
                }
                if (generation > _pendingGeneration)
                {
                    _pendingGeneration = generation;
                    _bootstrapPlayers = null;
                    _bootstrapMonsters = null;
                    _playerBootstrapEnd = false;
                    _monsterBootstrapEnd = false;
                }
                if (monsters)
                {
                    _bootstrapMonsters = monsterStates ?? Array.Empty<PacketHelper.MonsterFullField>();
                    _monsterBootstrapEnd = true;
                }
                else
                {
                    _bootstrapPlayers = players ?? Array.Empty<PacketHelper.FullStateField>();
                    _playerBootstrapEnd = true;
                }
                if (!_playerBootstrapEnd || !_monsterBootstrapEnd || _bootstrapPlayers == null || _bootstrapMonsters == null)
                    return;
                bool hasSelf = false;
                foreach (var state in _bootstrapPlayers)
                    if (state.zoneInternalID == _pendingZoneInternalID) { hasSelf = true; break; }
                if (!hasSelf) return; // The periodic timeout requests a fresh, complete baseline.

                _zoneID = _pendingZoneID;
                _zoneGeneration = _pendingGeneration;
                _zoneInternalID = _pendingZoneInternalID;
                _viewData.SetPosition(_pendingStartX, _pendingStartY);
                _entityProfiles.Clear();
                foreach (var state in _bootstrapPlayers)
                {
                    _entityProfiles[state.zoneInternalID] = state.profileId;
                    ObserveProfile(state.profileId, state.profileVersion);
                    if (state.zoneInternalID == _pendingZoneInternalID)
                        _selfHP = state.hp;
                }
                RequestProfiles();
                OnZoneChageReceived?.Invoke(_zoneID, _pendingChatID, _zoneInternalID, _pendingStartX, _pendingStartY);
                OnFullReceived?.Invoke((ushort)_bootstrapPlayers.Length, _bootstrapPlayers);
                OnMonsterFullReceived?.Invoke((ushort)_bootstrapMonsters.Length, _bootstrapMonsters);
                lock (_sessionGate)
                {
                    _pendingOwnershipEpoch = 0;
                    _pendingBootstrapID = 0;
                    _respawnPending = false;
                    _respawnRetryDeadline = 0;
                    _awaitingZoneAck = false;
                    _worldReady = true;
                }
            });
        }
        public void ActionResultReceived(ushort zoneID, ulong generation, ushort count, PacketHelper.ActionResultField[] actions)
        {
            if (_lifetime.IsCancellationRequested) return;
            var subscribers = OnActionResultReceived;
            _threadDispatcher.Post(() =>
            {
                if (AcceptSimulation(zoneID, generation) && _worldReady) subscribers?.Invoke(count, actions);
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
            lock (_sessionGate)
            {
                _worldReady = false;
                _zonePending = false;
                _respawnPending = false;
                _respawnRetryDeadline = 0;
                _awaitingZoneAck = false;
                _inventoryPending = false;
            }
            CancelBazaar();
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
