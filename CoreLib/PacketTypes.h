#pragma once

#include <cstdint>
#include <string_view>
#include "Config.h"

namespace Core {
    constexpr inline const uint8_t FLAG_SIMULATION = 0x01;
    constexpr inline const uint16_t FIELD_COUNT = 6;
    constexpr inline const uint16_t MAX_DEFINED_OPCODE = 18;

    enum OP : uint16_t {
        AUTH = 1,
        AUTH_RESPONSE = 2,
        CHARACTER_LIST = 3,
        CHARACTER_LIST_RESPONSE = 4,
        ENTER_WORLD = 5,
        ENTER_WORLD_RESPONSE = 6,

        ZONE_FULL_STATE_BROADCAST = 7,
        ZONE_DELTA_UPDATE_BROADCAST = 8,

        ACTION = 9,                 // 이동 + 공격 + 스킬 사용 등 행동 패킷
        CHAT = 10,
        CHAT_BROADCAST = 11,

        ZONE_CHANGE = 12,
        ZONE_CHANGE_RESPONSE = 13,

        INVENTORY_UPDATE = 14,
        INVENTORY_UPDATE_RES = 15,
        INVENTORY_REQ = 16,
        INVENTORY_RES = 17,
        PING = 17, // 서버 송신
        PONG = 18, // 클라이언트 송신
    };

    enum ZONE_CHANGE: uint8_t {
        ENTER = 0,
        UP = 1,
        DOWN = 2,
        LEFT = 3,
        RIGHT = 4,
    };

    enum RES_STATUS : uint8_t {
        FAILED = 0,
        SUCCESS = 1,
    };

#pragma pack(push, 1)

    //  네트워크 계층에서 패킷을 열어봐야한다면 htonl, htons로 변환해서 써야됨.
    struct PacketHeader {
        uint16_t magic = MAGIC;
        uint16_t length;
        uint8_t opcode;
        uint8_t flags = 0x00; // 첫번째 비트는 시뮬레이션 로직인지 나타냄, 0x01 ~ 0x80
    };

    struct AuthRequestBody {
        uint64_t userID;
        uint8_t sessionToken[37];
    };

    struct AuthResponseBody {
        uint8_t resStatus;
    };

    struct CharacterInfo {
        uint64_t characterID;
        uint8_t name[MAX_CHARNAME_LEN];
        uint16_t level;
    };

    struct CharacterListResponseBody {
        uint8_t resStatus;
        uint8_t count;
        CharacterInfo characters[MAX_CHARACTER];
    };

    struct EnterWorldRequestBody {
        uint64_t characterID;
    };

    struct EnterWorldResponseBody {
        uint8_t resStatus;
        uint8_t name[MAX_CHARNAME_LEN];
        uint16_t level;
        uint32_t exp;
        int16_t hp;
        int16_t mp;
        uint8_t dir;
        float startX, startY;
        uint16_t currentZone;
    };

    struct InventoryUpdateResBody {
        uint8_t resStatus;
		uint32_t itemID;
        uint16_t itemQuantity;
        uint16_t slot;
    };

    struct InventoryItem {
        uint32_t itemID;
        uint16_t quantity;
        uint8_t  slot;
    };

    struct InventoryResBody {
        uint8_t resStatus;
        uint16_t itemCount;
        InventoryItem items[MAX_INVENTORY_ITEMS];
    };

    struct ZoneChangeBody {
        uint8_t op; // 0: 로비 zone에서 진입, 1 ~ 4: 상하좌우 인접 zone으로 이동
    };

    struct ZoneChangeResponseBody {
        uint8_t resStatus;
        uint16_t zoneID;
        uint64_t zoneInternalID;
        float startX, startY;
    };

    struct ChatRequestBody {
        uint16_t messageLength;
    }; // + raw char*

    struct ChatFloodEntity {
        uint64_t zoneInternalID; // sender
        uint16_t offset;
        uint16_t messageLength;
    };

    struct ChatFloodBody {
        uint16_t chatCnt;
        uint16_t totalMessageLength;
        ChatFloodEntity entities[MAX_CHAT_PACKET];
    }; // message 이어붙여서 쓰기


    struct ActionRequestBody {
        uint8_t dir; // 상, 하, 좌, 우
        float speed;
    };

    struct DeltaUpdateField {
        uint64_t zoneInternalID;   
        uint16_t fieldID;
        uint64_t fieldVal; // field에 맞는 타입으로 변환 해서 사용
    };

    struct DeltaSnapshotBody {
        uint16_t count;
        DeltaUpdateField updates[DELTA_UPDATE_COUNT];
    };

    struct FullStateField {
        uint64_t zoneInternalID;
        int hp; // 0
        int mp; // 1
        uint16_t level; // 2
        uint32_t exp; // 3
        uint8_t dir;  // 4
        float x, y; // 5
        char charName[MAX_CHARNAME_LEN];
    };

    struct FullSnapshotBody {
        uint16_t count;
        FullStateField states[MAX_ZONE_CAPACITY];
    };

    template<typename T>
    struct PacketStruct {
        PacketHeader header;
        T body;
    };

    struct Ping {
        uint64_t serverTimeNs; // std::chrono::steady_clock::now().time_since_epoch().count()
        uint64_t rtt;
    };

    struct Pong {
        uint64_t serverTimeNs; // 클라이언트는 그대로 return
    };
    
#pragma pack(pop)
    
    inline PacketHeader* parseHeader(uint8_t* data) {
        return reinterpret_cast<PacketHeader*>(data);
    }

    template<typename T>
    inline T* parseBody(uint8_t* data) {
        return reinterpret_cast<T*>(data + sizeof(PacketHeader));
    }

    inline std::string_view GetMessageView(uint8_t* data, uint16_t messageLen) {
        const char* msgPtr = reinterpret_cast<const char*>(data + sizeof(PacketHeader) + sizeof(ChatRequestBody));
        return std::string_view(msgPtr, messageLen);
    }
}

