#pragma once

#include <cstdint>
#include <string_view>
#include <BaseLib/BackPressurePolicy.h>

#include "MessagePool.h"
#include "CharacterRequestContext.h"

namespace Core {
    class IMessageQueue;
    class MessageWriter;
    struct CharacterState;

    class CacheCommandSender {
        IMessageQueue* queue = nullptr;
        const MessageWriter* writer = nullptr;
        CoreMessagePool* messagePool = nullptr;

        void Initialize(IMessageQueue* messageQueue, const MessageWriter* messageWriter, CoreMessagePool* pool) {
            queue = messageQueue;
            writer = messageWriter;
            messagePool = pool;
        }
        bool IsReady() const;

        template<typename Body>
        Base::EnqueueResult EnqueuePooled(uint64_t sessionID, uint16_t messageType, const Body& body) const;

        friend class Initializer;

    public:
        Base::EnqueueResult EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) const;
        Base::EnqueueResult EnqueueGoldDeposit(uint64_t sessionID, uint64_t characterID, uint64_t gold) const;

        Base::EnqueueResult EnqueueCharacterList(uint64_t sessionID, uint8_t channelID, uint64_t userID) const;
        Base::EnqueueResult EnqueueCharacterState(uint64_t sessionID, uint8_t channelID, uint64_t userID, uint64_t characterID) const;
        Base::EnqueueResult EnqueueProfileRename(uint64_t sessionID, uint32_t profileID, std::string_view name) const;
        Base::EnqueueResult EnqueueInventory(uint64_t sessionID, uint64_t characterID) const;
        Base::EnqueueResult EnqueueGoldBalance(const CharacterRequestContext& context) const;
        Base::EnqueueResult EnqueueDiamondBalance(const CharacterRequestContext& context) const;

        Base::EnqueueResult EnqueueBazaarBuy(const CharacterRequestContext& context, uint64_t listingID) const;
        Base::EnqueueResult EnqueueBazaarRegister(const CharacterRequestContext& context, uint16_t quantity, uint64_t totalDiamondPrice,
            uint8_t inventorySlot) const;
        Base::EnqueueResult EnqueueBazaarOutboxList(const CharacterRequestContext& context) const;
        Base::EnqueueResult EnqueueBazaarCheckOutbox(const CharacterRequestContext& context) const;
        Base::EnqueueResult EnqueueBazaarCancelListing(const CharacterRequestContext& context, uint64_t listingID) const;
        Base::EnqueueResult EnqueueBazaarClaim(const CharacterRequestContext& context, uint64_t listingID) const;
        Base::EnqueueResult EnqueueBazaarMyList(const CharacterRequestContext& context) const;
        Base::EnqueueResult EnqueueBazaarSearch(uint64_t sessionID, uint32_t requestID, uint16_t itemType, uint16_t page) const;
    };
}
