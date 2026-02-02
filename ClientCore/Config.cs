using System;
using System.Collections.Generic;
using System.Text;

namespace ClientCore
{
    internal class Config
    {
        public const int GAME_TICK = 50;
        public const int RECV_TICK = 25;

        // service
        public const string LOGIN_SERVER_ADDR = "http://127.0.0.1:3000/auth/login";


        // packet
        internal const int MAX_CHARNAME_LEN = 32;
        internal const int MAX_CHARACTER = 10;
        internal const int MAX_INVENTORY_ITEMS = 128;
        internal const int MAX_INVENTORY = 64;
        internal const int MAX_CHAT_PACKET = 10;
        internal const int MAX_ZONE_CAPACITY = 500;
        internal const int FIELD_COUNT = 10;
        internal const ushort MAGIC = 0x1234;
        internal const byte FLAG_SIMULATION = 0x01;
        internal const int DELTA_UPDATE_COUNT = 1000;
        
    }
}
