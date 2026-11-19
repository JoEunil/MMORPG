#include "pch.h"
#include "CacheCommandSender.h"
#include "CharacterState.h"
#include "IMessageQueue.h"
#include "LoggerGlobal.h"
#include "Message.h"
#include "MessageTypes.h"
#include "MessageWriter.h"
#include <cstring>

namespace Core {
    bool CacheCommandSender::IsReady() const {
        if (queue == nullptr) {
            sysLogger->LogError("cache command sender", "message queue not initialized");
            return false;
        }
        if (writer == nullptr) {
            sysLogger->LogError("cache command sender", "message writer not initialized");
            return false;
        }
        if (messagePool == nullptr) {
            sysLogger->LogError("cache command sender", "message pool not initialized");
            return false;
        }
        return true;
    }

    bool CacheCommandSender::EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) const {
        if (queue == nullptr || writer == nullptr) {
            errorLogger->LogError("cache command sender", "character state sender not initialized",
                "sessionID", sessionID, "characterID", character.characterID);
            return false;
        }
        Message msg(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
        const MsgCharacterStateUpdateBody body{
            .charID = character.characterID,
            .profileId = character.profileId,
            .attack = character.attack,
            .level = character.level,
            .exp = character.exp,
            .hp = character.hp,
            .mp = character.mp,
            .maxHp = character.maxHp,
            .maxMp = character.maxMp,
            .dir = character.dir,
            .x = character.x,
            .y = character.y,
            .lastZone = static_cast<uint8_t>(character.lastZone)
        };
        if (!writer->Write(msg, {sessionID, MSG_CHARACTER_STATE_UPDATE}, body)) {
            errorLogger->LogError("cache command sender", "could not encode character state",
                "sessionID", sessionID, "characterID", character.characterID);
            return false;
        }
        if (queue->EnqueueMessage(&msg))
            return true;

        std::vector<std::byte> bytes(msg.GetLength());
        std::memcpy(bytes.data(), msg.GetBuffer(), bytes.size());
        errorLogger->LogError("cache command sender", "character state queue rejected",
            "sessionID", sessionID, "character state update message", bytes);
        return false;
    }

    bool CacheCommandSender::EnqueueGoldDeposit(uint64_t sessionID, uint64_t characterID, uint64_t gold) const {
        if (queue == nullptr || writer == nullptr)
            return false;
        Message msg(sizeof(MsgStruct<MsgCurrencyDepositBody>));
        const MsgCurrencyDepositBody body{characterID, gold};
        return writer->Write(msg, {sessionID, MSG_CURRENCY_DEPOSIT}, body)
            && queue->EnqueueMessage(&msg);
    }

    template<typename Body>
    bool CacheCommandSender::EnqueuePooled(uint64_t sessionID, uint16_t messageType, const Body& body) const {
        if (queue == nullptr || writer == nullptr || messagePool == nullptr)
            return false;
        auto message = messagePool->AcquireScoped();
        return message && writer->Write(*message, {sessionID, messageType}, body)
            && queue->EnqueueMessage(message.get());
    }

    bool CacheCommandSender::EnqueueCharacterList(uint64_t sessionID, uint8_t channelID, uint64_t userID) const {
        return EnqueuePooled(sessionID, MSG_CHARACTER_LIST_REQ, MsgCharacterListReqBody{channelID, userID});
    }

    bool CacheCommandSender::EnqueueCharacterState(uint64_t sessionID, uint8_t channelID, uint64_t userID, uint64_t characterID) const {
        return EnqueuePooled(sessionID, MSG_CHARACTER_STATE_REQ, MsgCharacterStateReqBody{channelID, userID, characterID});
    }

    bool CacheCommandSender::EnqueueProfileRename(uint64_t sessionID, uint32_t profileID, std::string_view name) const {
        MsgProfileRenameBody body{};
        if (name.empty() || name.size() >= sizeof(body.name))
            return false;
        body.profileId = profileID;
        std::memcpy(body.name, name.data(), name.size());
        return EnqueuePooled(sessionID, MSG_PROFILE_RENAME, body);
    }

    bool CacheCommandSender::EnqueueInventory(uint64_t sessionID, uint64_t characterID) const {
        return EnqueuePooled(sessionID, MSG_INVENTORY_REQ, MsgInventoryReqBody{characterID});
    }

    bool CacheCommandSender::EnqueueGoldBalance(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_CURRENCY_REQ, MsgCurrencyReqBody{characterID, requestID});
    }

    bool CacheCommandSender::EnqueueDiamondBalance(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_DIAMOND_REQ, MsgDiamondReqBody{characterID, requestID});
    }

    bool CacheCommandSender::EnqueueBazaarBuy(uint64_t sessionID, uint64_t characterID, uint64_t listingID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_BUY, MsgBazaarBuyBody{characterID, listingID, requestID});
    }

    bool CacheCommandSender::EnqueueBazaarRegister(uint64_t sessionID, uint64_t characterID, uint16_t quantity,
        uint64_t totalDiamondPrice, uint32_t requestID, uint8_t inventorySlot) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_REGISTER,
            MsgBazaarRegisterBody{characterID, 0, quantity, totalDiamondPrice, requestID, inventorySlot});
    }

    bool CacheCommandSender::EnqueueBazaarOutboxList(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_OUTBOX_LIST, MsgBazaarOutboxListBody{characterID, requestID});
    }

    bool CacheCommandSender::EnqueueBazaarCheckOutbox(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_CHECK_OUTBOX, MsgBazaarCheckOutboxBody{characterID, requestID});
    }

    bool CacheCommandSender::EnqueueBazaarCancelListing(uint64_t sessionID, uint64_t characterID, uint64_t listingID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_CANCEL_LISTING, MsgBazaarCancelListingBody{characterID, listingID, requestID});
    }

    bool CacheCommandSender::EnqueueBazaarClaim(uint64_t sessionID, uint64_t characterID, uint64_t listingID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_CLAIM, MsgBazaarClaimBody{characterID, listingID, requestID});
    }

    bool CacheCommandSender::EnqueueBazaarMyList(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_MY_LIST, MsgBazaarMyListBody{requestID, characterID});
    }

    bool CacheCommandSender::EnqueueBazaarSearch(uint64_t sessionID, uint32_t requestID, uint16_t itemType, uint16_t page) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_SEARCH, MsgBazaarSearchBody{requestID, itemType, page});
    }
}
