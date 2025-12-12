using System;

namespace ClientCore
{
    internal static class ClientSession
    {
        private static readonly object _lock = new object();

        private static ulong _userID;
        private static string _accessToken;
        private static string _sessionToken;

        public static ulong UserID
        {
            get { lock (_lock) return _userID; }
            set { lock (_lock) _userID = value; }
        }

        public static bool HasAccessToken
        {
            get { lock (_lock) return !string.IsNullOrEmpty(_accessToken); }
        }

        public static bool HasSessionToken
        {
            get { lock (_lock) return !string.IsNullOrEmpty(_sessionToken); }
        }

        public static string AccessToken
        {
            get { lock (_lock) return _accessToken; }
            set { lock (_lock) { _accessToken = value; } }
        }

        public static string SessionToken
        {
            get { lock (_lock) return _sessionToken; }
            set { lock (_lock) { _sessionToken = value; } }
        }
    }
}
