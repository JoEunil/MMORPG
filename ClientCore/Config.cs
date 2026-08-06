using System;
using System.Collections.Generic;
using System.Text;

namespace ClientCore
{
    internal class Config
    {
        public const int GAME_TICK = 50; // 1 tick: 50ms, 20fps

        // service
        public const string LOGIN_SERVER_ADDR = "http://127.0.0.1:3000/auth/login";


        // packet
        internal const int MAX_CHARNAME_LEN = 32;
        internal const int MAX_CHARACTER = 10;
        internal const int MAX_INVENTORY_ITEMS = 128;
        internal const int MAX_INVENTORY = 64;
        internal const int MAX_CHAT_PACKET = 10;
        // 서버(CoreLib/Config.h)와 값을 일치시킨다.
        // 스냅샷은 DeserializeVariablePacket이 count만큼만 읽으므로 이 값들은 실제 파싱에 쓰이지 않지만,
        // 프로토콜 상수가 서버와 어긋나 있으면 나중에 참조할 때 조용히 틀린다.
        internal const int MAX_ZONE_CAPACITY = 2000;
        internal const int FIELD_COUNT = 10;
        internal const ushort MAGIC = 0xABCD;
        internal const byte FLAG_SIMULATION = 0x01;
        internal const int DELTA_UPDATE_COUNT = 5000;
        internal const int ACTION_RESULT_COUNT = 500;
        internal const int MAX_MONSTER_DELTA = 3000;
        internal const int MAX_MONSTER_COUNT = 1000;
        internal const byte NONE_SKILL = 255;
    }
}
