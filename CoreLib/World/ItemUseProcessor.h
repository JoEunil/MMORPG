#pragma once
#include "../Packet/PacketTypes.h"

namespace Data {
    struct ItemDataPotion;
}

namespace Core {
    struct CharacterState;
    class IInventoryOperations;

    struct ItemUseOutcome {
        ItemUseResponseBody response;
        bool changed = false;
    };

    class ItemUseProcessor {
        static ItemUseOutcome ApplyHPPotion(CharacterState& character, const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory);
        static ItemUseOutcome ApplyMPPotion(CharacterState& character, const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory);
        static ItemUseOutcome ConsumePotion(CharacterState& character, const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory, bool resourceFull);

    public:
        static ItemUseOutcome Apply(CharacterState& character, const ItemUseRequestBody& request, IInventoryOperations* inventory);
    };
}
