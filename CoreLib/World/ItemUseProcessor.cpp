#include "pch.h"
#include "ItemUseProcessor.h"
#include "ItemUseState.h"
#include "CharacterState.h"
#include "../Interface/IInventoryOperations.h"
#include "../Data/ItemData.h"
#include <algorithm>

namespace Core {
    ItemUseOutcome ItemUseProcessor::Apply(ItemUseState& state, CharacterState& character, const ItemUseRequestBody& request, IInventoryOperations* inventory) {
        ItemUseResponseBody result{request.requestID, request.itemID, 0, 0, 0};
        if (request.requestID == 0 || request.itemID == 0)
            return {result};
        const auto& previous = state.lastResponse;
        if (request.requestID == previous.requestID) {
            if (request.itemID == previous.itemID)
                return {previous};
            return {result};
        }
        if (request.requestID < previous.requestID) {
            result.status = 7;
            return {result};
        }
        const auto* item = Data::FindItem(request.itemID);
        const auto* potion = item == nullptr ? nullptr : std::get_if<Data::ItemDataPotion>(&item->details);
        ItemUseOutcome outcome{result};
        outcome.response.status = 9;
        if (potion != nullptr) {
            switch (potion->type) {
            case Data::PotionType::HP:
                outcome = ApplyHPPotion(state, character, request, *potion, inventory);
                break;
            case Data::PotionType::MP:
                outcome = ApplyMPPotion(state, character, request, *potion, inventory);
                break;
            default:
                break;
            }
        }
        state.lastResponse = outcome.response;
        return outcome;
    }

    ItemUseOutcome ItemUseProcessor::ApplyHPPotion(ItemUseState& state, CharacterState& character,
        const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory) {
        const auto outcome = ConsumePotion(state, character, request, potion, inventory, character.hp >= character.maxHp);
        if (outcome.changed)
            character.hp = static_cast<int>((std::min)(static_cast<int64_t>(character.maxHp), static_cast<int64_t>(character.hp) + potion.recovery));
        return outcome;
    }

    ItemUseOutcome ItemUseProcessor::ApplyMPPotion(ItemUseState& state, CharacterState& character,
        const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory) {
        const auto outcome = ConsumePotion(state, character, request, potion, inventory, character.mp >= character.maxMp);
        if (outcome.changed)
            character.mp = static_cast<int>((std::min)(static_cast<int64_t>(character.maxMp), static_cast<int64_t>(character.mp) + potion.recovery));
        return outcome;
    }

    ItemUseOutcome ItemUseProcessor::ConsumePotion(ItemUseState& state, CharacterState& character,
        const ItemUseRequestBody& request, const Data::ItemDataPotion& potion, IInventoryOperations* inventory, bool resourceFull) {
        ItemUseResponseBody result{request.requestID, request.itemID, 0, 0, 0};
        bool changed = false;
        auto& cooldown = state.cooldownTicks[static_cast<std::size_t>(potion.type)];
        result.cooldownRemainingTicks = cooldown;
        if (character.hp <= 0)
            result.status = 2;
        else if (resourceFull)
            result.status = 3;
        else if (cooldown != 0)
            result.status = 4;
        else if (!inventory)
            result.status = 8;
        else {
            const auto consumed = inventory->ConsumeItem(character.characterID, request.itemID);
            result.remainingQuantity = consumed.remainingQuantity;
            switch (consumed.status) {
            case ItemOperationStatus::Success:
                cooldown = potion.cooldownTicks;
                character.dirtyBit |= 3;
                changed = true;
                result.status = 1;
                result.cooldownRemainingTicks = cooldown;
                break;
            case ItemOperationStatus::Missing:
                result.status = 5;
                break;
            case ItemOperationStatus::Busy:
                result.status = 6;
                break;
            default:
                result.status = 8;
                break;
            }
        }
        return {result, changed};
    }
}
