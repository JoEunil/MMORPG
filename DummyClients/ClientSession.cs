using ClientCore.Network;
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

        public ClientSession(TCPSocket sock) 
        {
            _sock = sock;
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
