using ClientCore.Network;
using ClientCore.PacketHelper;
using ClientCore.Services;
using System;
using System.Drawing;
using System.Net.Http;
using System.Threading.Tasks;
using System.Xml.Linq;


namespace ClientCore
{
    public class ViewModel : IViewModelUI, IViewModelNet, IViewModelTick
    {
        private readonly INetworkService _network;
        private readonly IViewDataModel _viewData;
        private readonly ILogger _logger;
        private readonly IMainThreadDispatcher _threadDispatcher;

        public event Action<bool> OnEnterSuccess;
        public event Action<bool> OnCharacterListReceived;
        public event Action<byte, ulong, string, string> OnChatReceived;
        public event Action<bool> OnInventoryReceived;
        public event Action<ushort, ulong, ulong, float, float> OnZoneChageReceived;
        public event Action<ushort, DeltaUpdateField[]> OnDeltaReceived;
        public event Action<ushort, FullStateField[]> OnFullReceived;
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
            try
            {
                var (ip, port) = await AuthService.Instance.GetSessionAsync();
                _logger.Log($"Session Token: {ClientSession.SessionToken}");
                await _network.Connect(ip, port);
            }
            catch (Exception ex)
            {
                _logger.Log($"{ex.Message}");
            }
        }

        // UI
        public async Task Login(string id, string pwd)
        {
            try
            {
                await AuthService.Instance.LoginAsync(id, pwd);
                _logger.Log($"로그인 성공 accessToken:{ClientSession.AccessToken}, userID: {ClientSession.UserID}");
                await Connect();
            }
            catch (Exception ex)
            {
                _logger.Log($"로그인 실패: {ex.Message}");
            }
        }
        public void Enter(ulong charID)
        {
            _network.Enter(charID);
        }

        public void Chat(string message, byte scope, ulong targetID)
        {
            _network.Chat(message, scope, targetID);
        }
        public void ZoneChange(int op)
        {
            _network.ZoneChange((byte)op);
        }

        // Client Tick 루프
        public void SendUpdate()
        {
            var res = _viewData.GetMoveState();
            if (res.Item1 == true)
            {
                _network.Move(res.Item2, res.Item3);
            }
        }

        // Network
        // Post 인자로 넘기는 lambda 함수 자체가 action으로 처리됨. 람다 함수가 action으로 암시적 형변환이 일어남.
        public void AuthReceived(byte resStatus)
        {
            bool success = resStatus != 0;
            if (success)
                _network.CharacterList();
            else
                _logger.Log("Auth request failed");
        }

        public void ChatacterListReceived(byte resStatus, ushort count, PacketHelper.CharacterInfo[] characters)
        {
            bool success = resStatus != 0;
            if (success)
            {
                _viewData.SetCharList(count, characters);
            }
            _threadDispatcher.Post(() =>
            {
                OnCharacterListReceived?.Invoke(success);
            });
        }

        public void EnterReceived(byte resStatus, byte[] name, ushort level, uint exp, short hp, short mp, byte dir, float startX, float startY, ushort currentZone)
        {
            bool success = resStatus != 0;
            if (success)
            {
                _network.ZoneChange((byte)ZONE_CHANGE.ENTER);
            }
            _threadDispatcher.Post(() =>
            {
                OnEnterSuccess?.Invoke(success);
            });

        }

        public void ChatReceived(Packet.Message message)
        {
            _threadDispatcher.Post(() =>
            {
                OnChatReceived?.Invoke(message.scope, message.senderID, message.senderName, message.message);
            });
        }

        public void InventoryReceived(byte resStatus, PacketHelper.InventoryItem[] items)
        {
            bool success = resStatus != 0;
            if (success)
            {
                _viewData.SetInventory(items);
            }
            _threadDispatcher.Post(() =>
            {
                OnInventoryReceived?.Invoke(success);
            });
        }

        public void ZoneChageReceived(byte resStatus, ushort zoneID, ulong chatID, ulong zoneInternalID, float x, float y)
        {
            bool success = resStatus != 0;
            if (success)
                _threadDispatcher.Post(() =>
                {
                    OnZoneChageReceived?.Invoke(zoneID, chatID, zoneInternalID, x, y);
                });
            else
            {
                OnZoneChageFailed?.Invoke();
                _logger.Log("Zone Change Failed");
            }
        }

        public void DeltaReceived(ushort count, PacketHelper.DeltaUpdateField[] updates)
        {
            _threadDispatcher.Post(() =>
            {
                OnDeltaReceived?.Invoke(count, updates);
            });
        }
        public void FullReceived(ushort count, PacketHelper.FullStateField[] states)
        {
            _threadDispatcher.Post(() =>
            {
                OnFullReceived?.Invoke(count, states);
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
            _logger.Log("Error: " + msg);
        }
        public void Log(string msg)
        {
            _logger.Log(msg);
        }
    }
}