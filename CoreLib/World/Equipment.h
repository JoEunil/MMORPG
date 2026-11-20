#pragma once

#include <cstdint>

namespace Core {
    class Equipment {
    public:
        Equipment() = default;
        Equipment(uint32_t weaponID, uint32_t shieldID, uint32_t bodyID)
            : m_weaponID(weaponID), m_shieldID(shieldID), m_bodyID(bodyID) {}

        uint32_t WeaponID() const { return m_weaponID; }
        uint32_t ShieldID() const { return m_shieldID; }
        uint32_t BodyID() const { return m_bodyID; }

        uint16_t AttackBonus() const;
        uint16_t EffectiveAttack(uint16_t baseAttack) const;
        uint8_t DefensePercent() const;
        uint64_t DamageAfterDefense(uint64_t damage) const;

    private:
        uint32_t m_weaponID = 0;
        uint32_t m_shieldID = 0;
        uint32_t m_bodyID = 0;
    };
}
