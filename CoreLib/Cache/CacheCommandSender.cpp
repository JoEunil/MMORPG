#include "pch.h"
#include "CacheCommandSender.h"
#include "../World/CharacterState.h"
#include "../Interface/IMessageQueue.h"
#include "../LoggerGlobal.h"
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

    Base::EnqueueResult CacheCommandSender::EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) const {
        if (queue == nullptr || writer == nullptr) {
            errorLogger->LogError("cache command sender", "character state sender not initialized", {{"sessionID", sessionID}, {"characterID",
                character.characterID}});
            return Base::EnqueueResult::Rejected;
        }
        Message msg(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
        const MsgCharacterStateUpdateBody body{.charID = character.characterID,
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
            .lastZone = static_cast<uint8_t>(character.lastZone)};
        if (!writer->Write(msg, {sessionID, MSG_CHARACTER_STATE_UPDATE}, body)) {
            errorLogger->LogError("cache command sender", "could not encode character state", {{"sessionID", sessionID}, {"characterID", character.characterID}});
            return Base::EnqueueResult::Rejected;
        }
        const auto result = queue->EnqueueMessage(&msg);
        if (result != Base::EnqueueResult::Rejected)
            return result;

        std::vector<std::byte> bytes(msg.GetLength());
        std::memcpy(bytes.data(), msg.GetBuffer(), bytes.size());
        errorLogger->LogError("cache command sender", "character state queue rejected", {{"sessionID", sessionID}, {"character state update message", bytes}});
        return Base::EnqueueResult::Rejected;
    }

    Base::EnqueueResult CacheCommandSender::EnqueueGoldDeposit(uint64_t sessionID, uint64_t characterID, uint64_t gold) const {
        if (queue == nullptr || writer == nullptr)
            return Base::EnqueueResult::Rejected;
        Message msg(sizeof(MsgStruct<MsgCurrencyDepositBody>));
        const MsgCurrencyDepositBody body{characterID, gold};
        if (!writer->Write(msg, {sessionID, MSG_CURRENCY_DEPOSIT}, body))
            return Base::EnqueueResult::Rejected;
        return queue->EnqueueMessage(&msg);
    }

    template <typename Body> Base::EnqueueResult CacheCommandSender::EnqueuePooled(uint64_t sessionID, uint16_t messageType, const Body& body) const {
        if (queue == nullptr || writer == nullptr || messagePool == nullptr)
            return Base::EnqueueResult::Rejected;
        auto message = messagePool->AcquireScoped();
        if (!message || !writer->Write(*message, {sessionID, messageType}, body))
            return Base::EnqueueResult::Rejected;
        return queue->EnqueueMessage(message.get());
    }

    Base::EnqueueResult CacheCommandSender::EnqueueCharacterList(uint64_t sessionID, uint8_t channelID, uint64_t userID) const {
        return EnqueuePooled(sessionID, MSG_CHARACTER_LIST_REQ, MsgCharacterListReqBody{channelID, userID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueCharacterState(uint64_t sessionID, uint8_t channelID, uint64_t userID, uint64_t characterID) const {
        return EnqueuePooled(sessionID, MSG_CHARACTER_STATE_REQ, MsgCharacterStateReqBody{channelID, userID, characterID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueProfileRename(uint64_t sessionID, uint32_t profileID, std::string_view name) const {
        MsgProfileRenameBody body{};
        if (name.empty() || name.size() >= sizeof(body.name))
            return Base::EnqueueResult::Rejected;
        body.profileId = profileID;
        std::memcpy(body.name, name.data(), name.size());
        return EnqueuePooled(sessionID, MSG_PROFILE_RENAME, body);
    }

    Base::EnqueueResult CacheCommandSender::EnqueueInventory(uint64_t sessionID, uint64_t characterID) const {
        return EnqueuePooled(sessionID, MSG_INVENTORY_REQ, MsgInventoryReqBody{characterID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueGoldBalance(const CharacterRequestContext& context) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_CURRENCY_REQ, MsgCurrencyReqBody{characterID, requestID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueDiamondBalance(const CharacterRequestContext& context) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_DIAMOND_REQ, MsgDiamondReqBody{characterID, requestID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarBuy(const CharacterRequestContext& context, uint64_t listingID) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_BAZAAR_BUY, MsgBazaarBuyBody{characterID, listingID, requestID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarRegister(const CharacterRequestContext& context, uint16_t quantity, uint64_t totalDiamondPrice,
        uint8_t inventorySlot) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_BAZAAR_REGISTER, MsgBazaarRegisterBody{characterID, 0, quantity, totalDiamondPrice, requestID, inventorySlot});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarOutboxList(const CharacterRequestContext& context) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_BAZAAR_OUTBOX_LIST, MsgBazaarOutboxListBody{characterID, requestID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarCheckOutbox(const CharacterRequestContext& context) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_BAZAAR_CHECK_OUTBOX, MsgBazaarCheckOutboxBody{characterID, requestID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarCancelListing(const CharacterRequestContext& context, uint64_t listingID) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_BAZAAR_CANCEL_LISTING, MsgBazaarCancelListingBody{characterID, listingID, requestID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarClaim(const CharacterRequestContext& context, uint64_t listingID) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_BAZAAR_CLAIM, MsgBazaarClaimBody{characterID, listingID, requestID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarMyList(const CharacterRequestContext& context) const {
        const auto& [sessionID, characterID, requestID] = context;
        return EnqueuePooled(sessionID, MSG_BAZAAR_MY_LIST, MsgBazaarMyListBody{requestID, characterID});
    }

    Base::EnqueueResult CacheCommandSender::EnqueueBazaarSearch(uint64_t sessionID, uint32_t requestID, uint16_t itemType, uint16_t page) const {
        return EnqueuePooled(sessionID, MSG_BAZAAR_SEARCH, MsgBazaarSearchBody{requestID, itemType, page});
    }
}
