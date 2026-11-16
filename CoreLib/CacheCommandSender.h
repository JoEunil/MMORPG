#pragma once

#include <cstdint>

namespace Core {
    class IMessageQueue;
    struct CharacterState;

    class CacheCommandSender {
        IMessageQueue* queue = nullptr;

        void Initialize(IMessageQueue* messageQueue) {
            queue = messageQueue;
        }
        bool IsReady() const;
        friend class Initializer;

    public:
        bool EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) const;
        bool EnqueueGoldDeposit(uint64_t sessionID, uint64_t characterID, uint64_t gold) const;
    };
}
