#pragma once
#include "../Packet/PacketTypes.h"

namespace Data {
    struct ItemDataPotion;
}

namespace Core {
    struct CharacterState;
    struct ItemUseState;
    class IInventoryOperations;

    struct ItemUseOutcome {
        ItemUseResponseBody response;
        bool changed = false;
    };

    class ItemUseProcessor {
        static ItemUseOutcome ApplyHPPotion(ItemUseState& state, CharacterState& character,
            const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory);
        static ItemUseOutcome ApplyMPPotion(ItemUseState& state, CharacterState& character,
            const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory);
        static ItemUseOutcome ConsumePotion(ItemUseState& state, CharacterState& character,
            const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory, bool resourceFull);

    public:
        static ItemUseOutcome Apply(ItemUseState& state, CharacterState& character,
            const ItemUseRequestBody& request, IInventoryOperations* inventory);
    };
}
