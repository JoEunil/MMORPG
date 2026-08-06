using ClientCore.Network;
using ClientCore.PacketHelper;
using System;

namespace ClientCore
{
    internal class ClientSession
    {
        private readonly object _lock = new object();

        private TCPSocket _sock;
        private ulong _userID;
        private string _accessToken;
        private string _sessionToken;

        // Dummy에 viewData가 없으므로, ClientSession에서 좌표를 관리한다.
        private float _x;
        private float _y;
        private bool _hasPosition;

        public ClientSession(TCPSocket sock)
        {
            _sock = sock;
        }

        public void SetPosition(float x, float y)
        {
            lock (_lock) { _x = x; _y = y; _hasPosition = true; }
        }

        public bool HasPosition
        {
            get { lock (_lock) return _hasPosition; }
        }

        public byte[]? BuildMovePacket(byte dir, float step)
        {
            float x, y;
            lock (_lock)
            {
                if (!_hasPosition)
                    return null;
                switch (dir)
                {
                    case 0: _y += step; break;
                    case 1: _y -= step; break;
                    case 2: _x -= step; break;
                    case 3: _x += step; break;
                }
                x = _x;
                y = _y;
            }
            return PacketBuilder.CreateActionPacketFast(dir, x, y, Config.NONE_SKILL);
        }
        public TCPSocket GetSocket()
        {
            return _sock;
        }
        public ulong UserID
        {
            get { lock (_lock) return _userID; }
            set { lock (_lock) _userID = value; }
        }

        public bool HasAccessToken
        {
            get { lock (_lock) return !string.IsNullOrEmpty(_accessToken); }
        }

        public bool HasSessionToken
        {
            get { lock (_lock) return !string.IsNullOrEmpty(_sessionToken); }
        }

        public string AccessToken
        {
            get { lock (_lock) return _accessToken; }
            set { lock (_lock) { _accessToken = value; } }
        }

        public string SessionToken
        {
            get { lock (_lock) return _sessionToken; }
            set { lock (_lock) { _sessionToken = value; } }
        }
    }
}
