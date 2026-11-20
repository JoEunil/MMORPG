#include "pch.h"
#include <cmath>
#include <limits>
#include "ZoneState.h"
#include "../Interface/IPacket.h"
#include "../Interface/ILogger.h"
#include "../Packet/PacketTypes.h"
#include "../Packet/PacketWriter.h"
#include "SessionManager.h"
#include "WorldMap.h"
#include "Interface/IZoneTransfer.h"
#include "../Interface/IInventoryOperations.h"

namespace Core {
    void ZoneState::ApplyAction(uint64_t sessionID, const ActionRequestBody& action,
        IZoneTransfer& zoneTransfer) {
        float previousX, previousY, previousBudget;
        uint8_t previousDir;
        uint32_t previousDirty;
        {
            std::lock_guard zoneLock(m_mutex);
            const auto member = m_sessionToIndex.find(sessionID);
            if (member == m_sessionToIndex.end())
                return;
            const auto& character = m_chars[member->second];
            previousX = character.x;
            previousY = character.y;
            previousBudget = character.moveBudget;
            previousDir = character.dir;
            previousDirty = character.dirtyBit;
        }

        Move(sessionID, action.dir, action.x, action.y);
        uint16_t destination = 0;
        std::shared_ptr<ZoneTransfer> transfer;
        {
            std::lock_guard zoneLock(m_mutex);
            const auto member = m_sessionToIndex.find(sessionID);
            if (member == m_sessionToIndex.end())
                return;
            const auto& character = m_chars[member->second];
            destination = action.ownershipEpoch == (std::numeric_limits<uint64_t>::max)()
                ? 0 : zoneTransfer.FindDestination(m_zoneID, character.x, character.y);
            if (destination != 0) {
                const float x = character.x, y = character.y, moveBudget = character.moveBudget;
                const uint8_t dir = character.dir;
                RestoreMovementLocked(sessionID, previousX, previousY, previousDir, previousBudget, previousDirty);

                transfer = std::make_shared<ZoneTransfer>();
                transfer->character = character;
                transfer->sourceZoneID = m_zoneID;
                transfer->ownershipEpoch = action.ownershipEpoch;
                transfer->x = x;
                transfer->y = y;
                transfer->moveBudget = moveBudget;
                transfer->dir = dir;
                transfer->skillSlot = action.skillSlot;
                transfer->aimAngle = action.aimAngle;
            }
        }

        if (transfer) {
            TryStartZoneTransfer(destination, transfer, zoneTransfer);
            return;
        }

        Skill(sessionID, action.skillSlot, action.aimAngle);
        DirtyCheck(sessionID);
    }

    // 틱마다 이동 예산을 보충한다. Move()가 이 예산에서 이동 거리를 차감하므로,
    // 한 틱에 입력이 몇 개 들어오든 총 이동량이 경과 시간을 넘지 못한다.
    // CAP으로 이월을 제한해 오래 정지했다가 한 번에 순간이동하는 것을 막는다.
    void ZoneState::ReplenishMoveBudget() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& character : m_chars) {
            character.moveBudget += MOVE_BUDGET_PER_TICK;
            if (character.moveBudget > MOVE_BUDGET_CAP)
                character.moveBudget = MOVE_BUDGET_CAP;
        }
    }

    void ZoneState::Move(uint64_t sessionID, uint8_t dir, float x, float y) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }
        auto& character = m_chars[it->second];

        //clamp
        if (dir > 3 || !std::isfinite(x) || !std::isfinite(y)) {
            m_cheatList.emplace_back(sessionID, 1);
            return;
        }

        // 방향은 좌표와 무관하게 반영 (제자리에서 바라보는 방향만 바꾸는 경우)
        if (character.dir != dir) {
            character.dir = dir;
            character.dirtyBit |= 0x40; // dir
        }

        // zone 경계로 클램프. 경계를 넘는 입력은 치트가 아니라 정상 흐름에서도 발생하므로 거부하지 않고 잘라낸다.
        // 클램프는 거리를 줄이기만 하므로 아래 예산 검사를 우회할 수 없다.
        if (x < m_area.x_min) x = m_area.x_min;
        else if (x > m_area.x_max) x = m_area.x_max;
        if (y < m_area.y_min) y = m_area.y_min;
        else if (y > m_area.y_max) y = m_area.y_max;

        const float dx = x - character.x;
        const float dy = y - character.y;
        const float distSq = dx * dx + dy * dy;
        if (distSq == 0.0f)
            return; // 이동 없음

        float dist = std::sqrt(distSq);
        const float budget = character.moveBudget + MOVE_BUDGET_EPSILON;
        if (dist > budget) {
            // 초과분은 거부하지 않고 예산만큼만 이동시킨다.
            // 거부한다면 간격이 MOVE_BUDGET_CAP을 넘는 순간 이후 모든 이동이 영구히 거부된다.
            const float scale = budget / dist;
            x = character.x + dx * scale;
            y = character.y + dy * scale;
            dist = budget;

            // 다만 어떤 타이밍으로도 나올 수 없는 거리는 조작으로 본다.
            // 틱 지연으로 생기는 일시적 초과는 여기 걸리지 않는다.
            if (distSq > MOVE_BUDGET_CAP * MOVE_BUDGET_CAP) {
                gameLogger->LogInfo("zone state", "move exceeds budget", "sessionID", sessionID,
                    "dist", std::sqrt(distSq), "budget", character.moveBudget);
                m_cheatList.emplace_back(sessionID, 1);
            }
        }

        // A blocked map step is normal input, not a cheat, and spends no movement budget.
        // Check the budget-clamped segment, not just its endpoint (thin walls cannot be skipped).
        if (!WorldMap::CanTraverse(character.x, character.y, x, y))
            return;

        character.moveBudget -= dist;
        if (character.moveBudget < 0.0f)
            character.moveBudget = 0.0f;

        const bool movedX = (x != character.x);
        const bool movedY = (y != character.y);
        character.x = x;
        character.y = y;
        if (movedX)
            character.dirtyBit |= 0x80; // x
        if (movedY)
            character.dirtyBit |= 0x100; // y

        auto [newX, newY] = GetCell(character.x, character.y, m_area);

        uint16_t oldX = character.cellX;
        uint16_t oldY = character.cellY;
        uint16_t oldIdx = character.cellIdx;
        // cell 변경 발생할 경우만
        if (newX != oldX || newY != oldY)
        {
            RemoveFromCell(character);
            AddToCell(character, newX, newY);
        }
    }

    static const uint8_t NONE_SKILL = 255;
    void ZoneState::Skill(uint64_t sessionID, uint8_t skillSlot, float aimAngle) {
        if (skillSlot == NONE_SKILL || !std::isfinite(aimAngle) || aimAngle < 0 || aimAngle >= 360) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }
        auto& character = m_chars[it->second];
        if (skillSlot >= character.skillSlotCnt || skillSlot >= character.skillSlot.size()) {
            gameLogger->LogInfo("zone state", "invalid skill slot", "sessionID", sessionID, "skillSlot", skillSlot);
            m_cheatList.emplace_back(sessionID, 1);
            return;
        }
        auto& skill = character.skillSlot[skillSlot];
        if (skill.skillID >= Data::skillList.size()) {
            m_cheatList.emplace_back(sessionID, 1);
            return;
        }
        const Data::SkillData& skillInfo = Data::skillList[skill.skillID];

        if (skill.skillCoolDownTick != 0 || character.mp < skillInfo.mana) // 스킬 시전 불가능한 상태
            return;
        character.mp -= skillInfo.mana;
        if (skillInfo.mana != 0) {
            character.dirtyBit |= 0x02;
        }
        skill.skillCoolDownTick = skillInfo.coolDown;
        m_cells[character.cellY][character.cellX].activeSkills.emplace_back(
            0, sessionID, character.zoneInternalID, 0, skillSlot, character.dir, character.x, character.y, skill.skillID, 0, 0, aimAngle
        );
    }

    ParryResponseBody ZoneState::Parry(uint64_t sessionID, const ParryRequestBody& request) {
        ParryResponseBody result{request.requestID, request.ownershipEpoch, static_cast<uint8_t>(ParryStatus::Unavailable), 0, 0};
        std::lock_guard zoneLock(m_mutex);
        const auto member = m_sessionToIndex.find(sessionID);
        if (member == m_sessionToIndex.end())
            return result;

        auto& character = m_chars[member->second];
        auto& previous = character.lastParry;
        if (request.requestID < previous.requestID
            || (request.requestID == previous.requestID && request.ownershipEpoch != previous.ownershipEpoch)) {
            result.status = static_cast<uint8_t>(ParryStatus::StaleRequest);
            return result;
        }
        if (request.requestID == previous.requestID) {
            result.status = previous.status;
        } else {
            if (character.hp <= 0) result.status = static_cast<uint8_t>(ParryStatus::Dead);
            else if (character.parryCooldownTicks != 0) result.status = static_cast<uint8_t>(ParryStatus::Cooldown);
            else {
                character.parryWindowTicks = Data::parryWindowTicks;
                character.parryCooldownTicks = Data::parryCooldownTicks;
                result.status = static_cast<uint8_t>(ParryStatus::Accepted);
                m_cells[character.cellY][character.cellX].actionResults.push_back({0, character.zoneInternalID, 0,
                    Data::parrySkillSlot, character.dir, character.x, character.y, Data::parrySkillID, 0,
                    character.dir == 0 ? 0.0f : character.dir == 1 ? 180.0f : character.dir == 2 ? 270.0f : 90.0f});
            }
            previous = {result.requestID, result.ownershipEpoch, result.status};
        }
        if (result.status == static_cast<uint8_t>(ParryStatus::Accepted) || result.status == static_cast<uint8_t>(ParryStatus::Cooldown)) {
            result.windowRemainingTicks = character.parryWindowTicks;
            result.cooldownRemainingTicks = character.parryCooldownTicks;
        }
        return result;
    }

    void ZoneState::DirtyCheck(uint64_t sessionID) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }
        auto& character = m_chars[it->second];
        m_cells[character.cellY][character.cellX].dirtyChar.push_back(character.zoneInternalID);
    }

    std::optional<ItemUseResponseBody> ZoneState::UseItem(uint64_t sessionID, const ItemUseRequestBody& request,
        IInventoryOperations* inventory) {
        ItemUseResponseBody result{request.requestID, request.ownershipEpoch, request.itemID, 0, 0, 0};
        std::lock_guard zoneLock(m_mutex);
        const auto found = m_sessionToIndex.find(sessionID);
        if (found == m_sessionToIndex.end()) return std::nullopt;
        auto& character = m_chars[found->second];
        result.cooldownRemainingTicks = character.potionCooldownTicks;
        const auto item = Data::itemMap.find(request.itemID);
        if (item == Data::itemMap.end() || (item->second.hpRecovery == 0 && item->second.mpRecovery == 0)) result.status = 9;
        else if (character.hp <= 0) result.status = 2;
        else if (item->second.hpRecovery != 0 ? character.hp >= character.maxHp : character.mp >= character.maxMp) result.status = 3;
        else if (character.potionCooldownTicks != 0) result.status = 4;
        else if (!inventory) result.status = 8;
        else {
            const auto consumed = inventory->ConsumeItem(character.characterID, request.itemID);
            result.remainingQuantity = consumed.remainingQuantity;
            switch (consumed.status) {
            case ItemOperationStatus::Success:
                if (item->second.hpRecovery != 0)
                    character.hp = static_cast<int>((std::min)(static_cast<int64_t>(character.maxHp),
                        static_cast<int64_t>(character.hp) + item->second.hpRecovery));
                if (item->second.mpRecovery != 0)
                    character.mp = static_cast<int>((std::min)(static_cast<int64_t>(character.maxMp),
                        static_cast<int64_t>(character.mp) + item->second.mpRecovery));
                character.potionCooldownTicks = item->second.cooldownTicks;
                character.dirtyBit |= 3;
                m_cells[character.cellY][character.cellX].dirtyChar.push_back(character.zoneInternalID);
                result.status = 1;
                result.cooldownRemainingTicks = character.potionCooldownTicks;
                break;
            case ItemOperationStatus::Missing: result.status = 5; break;
            case ItemOperationStatus::Busy: result.status = 6; break;
            default: result.status = 8; break;
            }
        }
        return result;
    }

    std::optional<EquipmentResponseBody> ZoneState::Equipment(uint64_t sessionID, uint64_t requestID,
        uint64_t ownershipEpoch, bool write, uint8_t slot, uint32_t itemID, IInventoryOperations* inventory) {
        EquipmentResponseBody result{ requestID, ownershipEpoch, static_cast<uint8_t>(write ? 1 : 0), static_cast<uint8_t>(EquipmentStatus::Invalid), slot, 0, 0, 0, 0, 0, 0, 0 };
        std::lock_guard zoneLock(m_mutex);
        auto member = m_sessionToIndex.find(sessionID);
        if (member == m_sessionToIndex.end()) return std::nullopt;
        auto& character = m_chars[member->second];
        if (slot > 2 || (!write && slot != 0) || (write && itemID != 0 && itemID != 3u + slot)) result.status = static_cast<uint8_t>(EquipmentStatus::Unsupported);
        else if (write && character.hp <= 0) result.status = static_cast<uint8_t>(EquipmentStatus::Dead);
        else if (inventory == nullptr) result.status = static_cast<uint8_t>(EquipmentStatus::StorageFailure);
        else {
            const auto state = write ? inventory->EquipItem(character.characterID, slot, itemID) : inventory->GetEquipmentState(character.characterID);
            switch (state.status) {
            case EquipmentOperationStatus::Success:
                if ((state.weaponID != 0 && state.weaponID != 3) || (state.shieldID != 0 && state.shieldID != 4)
                    || (state.bodyID != 0 && state.bodyID != 5)
                    || (write && (slot == 0 ? state.weaponID : slot == 1 ? state.shieldID : state.bodyID) != itemID)) {
                    result.status = static_cast<uint8_t>(EquipmentStatus::StorageFailure);
                    break;
                }
                character.equippedWeaponItemID = state.weaponID;
                character.equippedShieldItemID = state.shieldID;
                character.equippedBodyItemID = state.bodyID;
                result.status = static_cast<uint8_t>(EquipmentStatus::Success);
                break;
            case EquipmentOperationStatus::Missing:
                result.status = static_cast<uint8_t>(EquipmentStatus::Missing);
                break;
            case EquipmentOperationStatus::Busy: result.status = static_cast<uint8_t>(EquipmentStatus::Busy); break;
            case EquipmentOperationStatus::Unsupported: result.status = static_cast<uint8_t>(EquipmentStatus::Unsupported); break;
            default: result.status = static_cast<uint8_t>(EquipmentStatus::StorageFailure); break;
            }
        }
        if (result.status == static_cast<uint8_t>(EquipmentStatus::Success)) {
            result.weaponID = character.equippedWeaponItemID;
            result.shieldID = character.equippedShieldItemID;
            result.bodyID = character.equippedBodyItemID;
            result.defensePercent = character.DefensePercent();
            result.baseAttack = character.attack;
            result.bonusAttack = character.WeaponAttackBonus();
            result.effectiveAttack = character.EffectiveAttack();
        }
        return result;
    }
}
