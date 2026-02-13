#pragma once
#include <cstdint>

#include "Config.h"

namespace Core {
    enum : uint16_t {
        MSG_CHARACTER_LIST_REQ = 1,
        MSG_CHARACTER_LIST_RES = 2,
        MSG_CHARACTER_STATE_REQ = 3,
        MSG_CHARACTER_STATE_RES = 4,
        MSG_CHARACTER_STATE_UPDATE = 5,
		MSG_INVENTORY_UPDATE = 6,
        MSG_INVENTORY_UPDATE_RES = 7,
		MSG_INVENTORY_REQ = 8,
		MSG_INVENTORY_RES = 9,
    };

    struct MsgHeader {
        uint64_t sessionID;
        uint16_t messageType;
    };

	template<typename T>
	struct MsgStruct {
		MsgHeader header;
		T body;
	};


	struct MsgCharacterListReqBody {
		uint8_t channelID;
		uint64_t userID;
	};

	struct MsgCharacterInfo {
		uint64_t characterID;
		uint8_t name[MAX_CHARNAME_LEN];
		uint16_t level;
	};

	struct MsgCharacterListResBody {
		uint8_t resStatus;
		uint8_t count;
		MsgCharacterInfo characters[MAX_CHARACTER];
	};

    struct MsgCharacterStateReqBody {
        uint8_t channelID;
        uint64_t userID;
        uint64_t characterID;
    };

    struct MsgCharacterStateResBody {
        uint8_t resStatus;
        uint64_t charID;
        uint8_t name[MAX_CHARNAME_LEN];
        uint16_t attack; // 기본 공격력
        uint16_t level;
        uint32_t exp;
        int hp;
        int mp;
        int maxHp;
        int maxMp;
        uint8_t dir;
        float startX, startY;
        uint8_t currentZone;
    };

    struct MsgCharacterStateUpdateBody {
        uint64_t charID;
        uint16_t attack; // 기본 공격력
        uint16_t level;
        uint32_t exp;
        int hp;
        int mp;
        int maxHp;
        int maxMp;
        uint8_t dir;
        float x, y;
        uint8_t lastZone;
    };

	struct MsgInventoryItem {
		uint32_t itemID;
		uint16_t quantity;
		uint8_t  slot;
	};

	struct MsgInventoryUpdateBody {
        uint64_t characterID;
		uint32_t itemID;
		uint8_t op; // 1: add, 2: delete, 3:update
		int16_t change; // 변화값
	};

	struct MsgInventoryUpdateResBody {
        uint8_t resStatus;
        uint64_t characterID;
		uint32_t itemID;
        uint16_t itemQuantity;
        uint16_t slot;
	};

    struct MsgInventoryReqBody {
        uint64_t characterID;
    };

	struct MsgInventoryResBody {
        uint8_t resStatus;
		uint16_t itemCount;
        MsgInventoryItem items[MAX_INVENTORY_ITEMS]; 
	};

	inline MsgHeader* parseMsgHeader(uint8_t* data) {
		return reinterpret_cast<MsgHeader*>(data);
	}

	template<typename T>
	inline T* parseMsgBody(uint8_t* data) {
		return reinterpret_cast<T*>(data + sizeof(MsgHeader));
	}
}   
