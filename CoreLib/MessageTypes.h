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
        MSG_BAZAAR_CHECK_OUTBOX = 30,
        MSG_BAZAAR_CHECK_OUTBOX_RES = 31,
        MSG_BAZAAR_CANCEL_LISTING = 34,
        MSG_BAZAAR_CANCEL_LISTING_RES = 35,

        // 쓰기는 큐 한 경로로 모아 profile_id별 순서를 보장한다.
        MSG_PROFILE_RENAME = 32,
        MSG_PROFILE_RENAME_RES = 33,
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
        uint32_t profileId;
        uint32_t profileVersion;
        uint8_t name[MAX_CHARNAME_LEN]; // 자기 이름은 진입 시 1회 내려준다.
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
        uint32_t profileId; // 캐시에서 profile을 내림
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
        uint32_t requestID;
    };

    struct MsgCurrencyResBody {
        uint8_t resStatus;
        uint64_t gold;
        uint32_t requestID;
        uint64_t characterID;
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
        uint32_t requestID;
    };

    struct MsgDiamondResBody {
        uint8_t resStatus;
        uint64_t diamond;
        uint64_t totalEarned;
        uint64_t totalSpent;
        uint32_t requestID;
        uint64_t characterID;
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
        uint8_t  status;  // 0=TRADING, 1=SOLD, 2=CLAIMED (not a claim entitlement)
    };

    inline constexpr uint16_t MAX_BAZAAR_MY_LIST = 30;
    inline constexpr uint16_t MAX_BAZAAR_SEARCH_RESULT = 30; 

    struct MsgBazaarMyListBody {
        uint32_t requestID;
        uint64_t characterID;
    };

    struct MsgBazaarMyListResBody {
        uint32_t requestID;
        uint64_t characterID; // Internal owner snapshot; never taken from the TCP request.
        uint8_t resStatus;
        uint16_t count;
        MsgBazaarListing listings[MAX_BAZAAR_MY_LIST];
    };

    struct MsgBazaarSearchBody {
        uint32_t requestID;
        uint16_t item_type;
        uint16_t  page;
    };

    struct MsgBazaarSearchResBody {
        uint32_t requestID;
        uint8_t resStatus;
        uint16_t count;
        MsgBazaarListing listings[MAX_BAZAAR_SEARCH_RESULT];
    };

    struct MsgBazaarRegisterBody {
        uint64_t characterID;
        uint32_t itemID;
        uint16_t quantity;
        uint64_t price;
        uint32_t requestID;
        uint8_t inventorySlot;
    };

    struct MsgBazaarRegisterResBody {
        uint8_t  resStatus; // 0: 실패(보상 완료), 1: 성공, 2: 아이템 수량 부족, 3: 골드 부족, 4: 유효하지 않은 아이템, 5: 결과/보상 미확인(자동 재시도 금지)
        uint32_t requestID;
        uint64_t characterID;
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
        uint32_t requestID;
    };

    struct MsgBazaarBuyResBody {
        uint8_t resStatus; // 0: unavailable, 1: bought, 3: SQL rollback, 4: insufficient, 5: outcome unknown
        uint64_t listingID;
        uint32_t itemID;
        uint32_t quantity;
        uint64_t diamondSpent;
        uint32_t requestID;
        uint64_t characterID;
    };

    struct MsgBazaarClaimBody {
        uint64_t characterID;
        uint64_t listingID;
        uint32_t requestID;
    };

    struct MsgBazaarClaimResBody {
        uint8_t resStatus;
        uint64_t listingID;
        uint64_t diamondClaimed;
        uint32_t requestID;
        uint64_t characterID;
    };

    struct MsgBazaarCheckOutboxBody {
        uint64_t characterID;
        uint32_t requestID;
    };

    struct MsgBazaarCheckOutboxResBody {
        uint8_t resStatus; // A cache pass, never a durable acknowledgement; failure may retain a prefix.
        uint32_t deliveredCount;
        uint32_t duplicatedCount;
        uint32_t blockedCount;
        uint32_t requestID;
        uint64_t characterID;
    };

    struct MsgBazaarCancelListingBody {
        uint64_t characterID;
        uint64_t listingID;
        uint32_t requestID;
    };

    struct MsgBazaarCancelListingResBody {
        uint8_t resStatus; // 1 confirms listing cancellation; item return delivery remains separate.
        uint32_t requestID;
        uint64_t listingID;
        uint64_t characterID;
    };

    struct MsgProfileRenameBody {
        uint32_t profileId;
        uint8_t  name[MAX_CHARNAME_LEN];
    };

    // DB에 확정된 version을 zone으로 되돌리기 위한 응답.
    struct MsgProfileRenameResBody {
        uint8_t  resStatus; //  0=실패(DB 오류), 1=성공, 2=거부(같은 profile_id에 rename 진행 중)
        uint32_t profileId;
        uint32_t version; // 실패 시 0
    };

	inline MsgHeader* parseMsgHeader(uint8_t* data) {
		return reinterpret_cast<MsgHeader*>(data);
	}

	template<typename T>
	inline T* parseMsgBody(uint8_t* data) {
		return reinterpret_cast<T*>(data + sizeof(MsgHeader));
	}
}   
