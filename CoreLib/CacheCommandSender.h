#pragma once

#include <cstdint>
#include <string_view>

#include "MessagePool.h"

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
        bool EnqueuePooled(uint64_t sessionID, uint16_t messageType, const Body& body) const;

        friend class Initializer;

    public:
        bool EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) const;
        bool EnqueueGoldDeposit(uint64_t sessionID, uint64_t characterID, uint64_t gold) const;

        bool EnqueueCharacterList(uint64_t sessionID, uint8_t channelID, uint64_t userID) const;
        bool EnqueueCharacterState(uint64_t sessionID, uint8_t channelID, uint64_t userID, uint64_t characterID) const;
        bool EnqueueProfileRename(uint64_t sessionID, uint32_t profileID, std::string_view name) const;
        bool EnqueueInventory(uint64_t sessionID, uint64_t characterID) const;
        bool EnqueueGoldBalance(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const;
        bool EnqueueDiamondBalance(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const;

        bool EnqueueBazaarBuy(uint64_t sessionID, uint64_t characterID, uint64_t listingID, uint32_t requestID) const;
        bool EnqueueBazaarRegister(uint64_t sessionID, uint64_t characterID, uint16_t quantity,
            uint64_t totalDiamondPrice, uint32_t requestID, uint8_t inventorySlot) const;
        bool EnqueueBazaarOutboxList(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const;
        bool EnqueueBazaarCheckOutbox(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const;
        bool EnqueueBazaarCancelListing(uint64_t sessionID, uint64_t characterID, uint64_t listingID, uint32_t requestID) const;
        bool EnqueueBazaarClaim(uint64_t sessionID, uint64_t characterID, uint64_t listingID, uint32_t requestID) const;
        bool EnqueueBazaarMyList(uint64_t sessionID, uint64_t characterID, uint32_t requestID) const;
        bool EnqueueBazaarSearch(uint64_t sessionID, uint32_t requestID, uint16_t itemType, uint16_t page) const;
    };
}
