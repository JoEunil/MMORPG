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

        MSG_CURRENCY_REQ = 10,
        MSG_CURRENCY_RES = 11,
        MSG_CURRENCY_DEPOSIT = 12,
        MSG_CURRENCY_DEPOSIT_RES = 13,

        MSG_DIAMOND_REQ = 14,
        MSG_DIAMOND_RES = 15,
        MSG_DIAMOND_DEPOSIT = 16,
        MSG_DIAMOND_DEPOSIT_RES = 17,

        MSG_BAZAAR_MY_LIST = 18,
        MSG_BAZAAR_MY_LIST_RES = 19,
        MSG_BAZAAR_SEARCH = 20, 
        MSG_BAZAAR_SEARCH_RES = 21, 
        MSG_BAZAAR_REGISTER = 22,
        MSG_BAZAAR_REGISTER_RES = 23,
        MSG_BAZAAR_CANCEL = 24,
        MSG_BAZAAR_CANCEL_RES = 25,
        MSG_BAZAAR_BUY = 26,
        MSG_BAZAAR_BUY_RES = 27,
        MSG_BAZAAR_CLAIM = 28,
        MSG_BAZAAR_CLAIM_RES = 29,
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
		uint8_t op; // 1: add, 2: update
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

    struct MsgCurrencyReqBody {
        uint64_t characterID;
    };

    struct MsgCurrencyResBody {
        uint8_t resStatus;
        uint64_t gold;
    };

    struct MsgCurrencyDepositBody {
        uint64_t characterID;
        uint64_t gold;
    };

    struct MsgCurrencyDepositResBody {
        uint8_t resStatus;
        uint64_t gold;
    };

    struct MsgDiamondReqBody {
        uint64_t characterID;
    };

    struct MsgDiamondResBody {
        uint8_t resStatus;
        uint64_t diamond;
        uint64_t totalEarned;
        uint64_t totalSpent;
    };

    struct MsgDiamondDepositBody {
        uint64_t characterID;
        uint64_t diamond;
    };

    struct MsgDiamondDepositResBody {
        uint8_t resStatus;
        uint64_t diamond;
    };

    struct MsgBazaarListing {
        uint64_t listingID;
        uint64_t sellerCharacterID;
        uint32_t itemID;
        uint16_t quantity;
        uint64_t price;        // 개당 가격
        uint64_t registeredAt; // unix timestamp
        uint8_t  status;  // 0=TRADING, 1=SOLD 
    };

    inline constexpr uint16_t MAX_BAZAAR_MY_LIST = 30;
    inline constexpr uint16_t MAX_BAZAAR_SEARCH_RESULT = 30; 

    struct MsgBazaarMyListBody {
        uint64_t characterID;
    };

    struct MsgBazaarMyListResBody {
        uint8_t resStatus;
        uint16_t count;
        MsgBazaarListing listings[MAX_BAZAAR_MY_LIST];
    };

    struct MsgBazaarSearchBody {
        uint16_t item_type;
        uint16_t  page;
    };

    struct MsgBazaarSearchResBody {
        uint8_t resStatus;
        uint16_t count;
        MsgBazaarListing listings[MAX_BAZAAR_SEARCH_RESULT];
    };

    struct MsgBazaarRegisterBody {
        uint64_t characterID;
        uint32_t itemID;
        uint16_t quantity;
        uint64_t price;
    };

    struct MsgBazaarRegisterResBody {
        uint8_t  resStatus; // 0: 실패, 1: 성공, 2: 아이템 수량 부족, 3: 골드 부족, 4: 유효하지 않은 아이템
    };

    struct MsgBazaarCancelBody {
        uint64_t characterID;
        uint64_t listingID;
    };

    struct MsgBazaarCancelResBody {
        uint8_t  resStatus;
    };

    struct MsgBazaarBuyBody {
        uint64_t characterID;
        uint64_t listingID;
    };

    struct MsgBazaarBuyResBody {
		uint8_t  resStatus; // 0: 실패, 1: 성공, 2: listingID 조회 실패, 3: 프로시저 실패, 4: 다이아 부족
        uint64_t listingID;
        uint32_t itemID;
        uint16_t quantity;
        uint32_t diamondSpent;
    };

    struct MsgBazaarClaimBody {
        uint64_t characterID; // 0: 실패, 1: 성공, 2: listingID 조회 실패, 3: 프로시저 실패, 4: CLAIM 실패
        uint64_t listingID;
    };

    struct MsgBazaarClaimResBody {
        uint8_t  resStatus;
        uint64_t listingID;
        uint32_t diamondClaimed;
    };

	inline MsgHeader* parseMsgHeader(uint8_t* data) {
		return reinterpret_cast<MsgHeader*>(data);
	}

	template<typename T>
	inline T* parseMsgBody(uint8_t* data) {
		return reinterpret_cast<T*>(data + sizeof(MsgHeader));
	}
}   
