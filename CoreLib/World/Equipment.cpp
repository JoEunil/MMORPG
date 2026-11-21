#include "pch.h"
#include "Equipment.h"

#include "../Data/ItemData.h"

namespace Core {
    uint16_t Equipment::AttackBonus() const {
        const auto* item = Data::FindItem(m_weaponID);
        const auto* weapon = item == nullptr ? nullptr : std::get_if<Data::ItemDataWeapon>(&item->details);
        return weapon == nullptr ? 0 : weapon->attackBonus;
    }

    uint16_t Equipment::EffectiveAttack(uint16_t baseAttack) const {
        const uint32_t total = static_cast<uint32_t>(baseAttack) + AttackBonus();
        return static_cast<uint16_t>(total > UINT16_MAX ? UINT16_MAX : total);
    }

    uint8_t Equipment::DefensePercent() const {
        uint16_t total = 0;
        if (const auto* item = Data::FindItem(m_shieldID)) {
            if (const auto* shield = std::get_if<Data::ItemDataShield>(&item->details))
                total += shield->defensePercent;
        }
        if (const auto* item = Data::FindItem(m_bodyID)) {
            if (const auto* armor = std::get_if<Data::ItemDataArmor>(&item->details))
                total += armor->defensePercent;
        }
        return static_cast<uint8_t>(total);
    }

    uint64_t Equipment::DamageAfterDefense(uint64_t damage) const {
        if (damage == 0)
            return 0;
        const uint64_t remainingPercent = 100 - DefensePercent();
        // Split before multiplication so even a wide incoming hit cannot overflow.
        const uint64_t reduced = damage / 100 * remainingPercent + damage % 100 * remainingPercent / 100;
        return reduced == 0 ? 1 : reduced;
    }
}
