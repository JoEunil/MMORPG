#pragma once
#include "CacheStorageInventory.h"
#include <array>
#include <cstring>
#include <istream>
#include <stdexcept>
#include <cstddef>

namespace Cache {
    // Persisted DB blob sizes, from the oldest layout to the current one.
    inline constexpr size_t LEGACY_ITEM_PREFIX_SIZE = 84;
    inline constexpr size_t LEGACY_RING_INVENTORY_SIZE = 352;
    inline constexpr size_t LEGACY_INVENTORY_SIZE = 360;
    inline constexpr size_t WEAPON_INVENTORY_SIZE = 368;
    inline constexpr size_t EQUIPMENT_INVENTORY_SIZE = 376;
    inline constexpr size_t LEGACY_EMPTY_SEED_SIZE = 2404;
    inline constexpr uint32_t WEAPON_INVENTORY_TAG = 0x31514549; // IEQ1

    inline constexpr uint16_t LEGACY_INVENTORY_WAL_TYPE = 1;
    inline constexpr uint16_t WEAPON_INVENTORY_WAL_TYPE = 2;
    inline constexpr uint16_t EQUIPMENT_INVENTORY_WAL_TYPE = 3;

    static_assert(offsetof(InventoryData, items) == 4 && sizeof(Core::MsgInventoryItem) == 8);
    static_assert(offsetof(InventoryData, recentEventIds) == 88);
    static_assert(offsetof(InventoryData, lastLsn) == 352);
    static_assert(offsetof(InventoryData, equipmentFormatTag) == LEGACY_INVENTORY_SIZE);
    static_assert(offsetof(InventoryData, equippedWeaponItemID) == 364);
    static_assert(offsetof(InventoryData, equippedShieldItemID) == 368);
    static_assert(offsetof(InventoryData, equippedBodyItemID) == 372);
    static_assert(sizeof(InventoryData) == EQUIPMENT_INVENTORY_SIZE);

    inline void ValidateInventoryImage(const InventoryData& data) {
        if (data.count > MAX_INVENTORY || data.head >= RING_SIZE || data.tail >= RING_SIZE
            || data.equipmentFormatTag != INVENTORY_EQUIPMENT_TAG)
            throw std::runtime_error("Invalid inventory image header.");

        uint16_t count = 0;
        bool weaponOwned = data.equippedWeaponItemID == 0;
        bool shieldOwned = data.equippedShieldItemID == 0;
        bool bodyOwned = data.equippedBodyItemID == 0;
        for (size_t i = 0; i < MAX_INVENTORY; ++i) {
            const auto& item = data.items[i];
            if (item.itemID == 0) {
                if (item.quantity != 0)
                    throw std::runtime_error("Empty inventory slot has quantity.");
                continue;
            }
            if (item.quantity == 0 || item.slot != i)
                throw std::runtime_error("Invalid inventory item slot.");
            // Slot identity is unique; multiple existing stacks may share an item ID.
            ++count;
            if (item.itemID == data.equippedWeaponItemID) weaponOwned = true;
            if (item.itemID == data.equippedShieldItemID) shieldOwned = true;
            if (item.itemID == data.equippedBodyItemID) bodyOwned = true;
        }
        if (count != data.count || !weaponOwned || !shieldOwned || !bodyOwned
            || (data.equippedWeaponItemID != 0 && data.equippedWeaponItemID != 3)
            || (data.equippedShieldItemID != 0 && data.equippedShieldItemID != 4)
            || (data.equippedBodyItemID != 0 && data.equippedBodyItemID != 5))
            throw std::runtime_error("Invalid inventory count or equipment slots.");
    }

    inline InventoryData DecodeInventory(const uint8_t* bytes, size_t size) {
        if (bytes == nullptr)
            throw std::runtime_error("Unsupported inventory image size.");

        // createCharacter.sql historically seeded an entirely zero 2404-byte inventory.
        // Accept only that exact sentinel, never guess a larger slot layout or truncate items.
        if (size == LEGACY_EMPTY_SEED_SIZE) {
            for (size_t i = 0; i < size; ++i)
                if (bytes[i] != 0)
                    throw std::runtime_error("Non-empty legacy seed inventory is unsupported.");
            return EMPTY_INVENTORY;
        }

        switch (size) {
        case LEGACY_ITEM_PREFIX_SIZE:
        case LEGACY_RING_INVENTORY_SIZE:
        case LEGACY_INVENTORY_SIZE:
        case WEAPON_INVENTORY_SIZE:
        case EQUIPMENT_INVENTORY_SIZE:
            break;
        default:
            throw std::runtime_error("Unsupported inventory image size.");
        }

        // Check the bool representation before copying untrusted persistent bytes into a bool.
        if (size >= LEGACY_RING_INVENTORY_SIZE && bytes[offsetof(InventoryData, lastOp)] > 1)
            throw std::runtime_error("Invalid inventory ring operation.");

        InventoryData data = EMPTY_INVENTORY;
        if (size == WEAPON_INVENTORY_SIZE) {
            uint32_t tag = 0;
            std::memcpy(&tag, bytes + offsetof(InventoryData, equipmentFormatTag), sizeof(tag));
            if (tag != WEAPON_INVENTORY_TAG)
                throw std::runtime_error("Invalid weapon-only inventory tag.");
            // Keep the current format tag and default values for shield and armor.
            std::memcpy(&data, bytes, LEGACY_INVENTORY_SIZE);
            std::memcpy(&data.equippedWeaponItemID, bytes + offsetof(InventoryData, equippedWeaponItemID),
                sizeof(data.equippedWeaponItemID));
        } else {
            std::memcpy(&data, bytes, size);
        }
        ValidateInventoryImage(data);
        return data;
    }

    inline InventoryData ReadInventory(std::istream& stream) {
        std::array<uint8_t, LEGACY_EMPTY_SEED_SIZE + 1> bytes{};
        stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        if (stream.bad())
            throw std::runtime_error("Inventory image read failed.");
        return DecodeInventory(bytes.data(), static_cast<size_t>(stream.gcount()));
    }

    inline std::vector<uint8_t> EncodeInventory(const InventoryData& data) {
        ValidateInventoryImage(data);
        std::vector<uint8_t> bytes(EQUIPMENT_INVENTORY_SIZE);
        std::memcpy(bytes.data(), &data, bytes.size());
        return bytes;
    }

    // Explicit offline rollback conversion only. Callers must first drain all newer WAL records.
    inline std::vector<uint8_t> EncodeLegacyInventory(const InventoryData& data) {
        ValidateInventoryImage(data);
        if (data.equippedWeaponItemID != 0 || data.equippedShieldItemID != 0 || data.equippedBodyItemID != 0)
            throw std::runtime_error("Unequip every slot before legacy conversion.");
        std::vector<uint8_t> bytes(LEGACY_INVENTORY_SIZE);
        std::memcpy(bytes.data(), &data, bytes.size());
        return bytes;
    }

    inline std::vector<uint8_t> EncodeInventoryWal(uint64_t characterID, const InventoryData& data) {
        if (characterID == 0)
            throw std::runtime_error("Invalid inventory WAL character.");
        auto image = EncodeInventory(data);
        std::vector<uint8_t> payload(sizeof(characterID) + image.size());
        std::memcpy(payload.data(), &characterID, sizeof(characterID));
        std::memcpy(payload.data() + sizeof(characterID), image.data(), image.size());
        return payload;
    }

    inline constexpr size_t GetInventoryWalImageSize(uint16_t type) noexcept {
        switch (type) {
        case LEGACY_INVENTORY_WAL_TYPE:
            return LEGACY_INVENTORY_SIZE;
        case WEAPON_INVENTORY_WAL_TYPE:
            return WEAPON_INVENTORY_SIZE;
        case EQUIPMENT_INVENTORY_WAL_TYPE:
            return EQUIPMENT_INVENTORY_SIZE;
        default:
            return 0;
        }
    }

    inline InventoryData DecodeInventoryWal(uint16_t type, const uint8_t* payload, size_t size, uint64_t& characterID) {
        const size_t imageSize = GetInventoryWalImageSize(type);
        if (payload == nullptr || imageSize == 0 || size != sizeof(characterID) + imageSize)
            throw std::runtime_error("Unsupported inventory WAL format.");
        std::memcpy(&characterID, payload, sizeof(characterID));
        if (characterID == 0)
            throw std::runtime_error("Invalid inventory WAL character.");
        return DecodeInventory(payload + sizeof(characterID), imageSize);
    }
}
