#pragma once
#include "CacheStorageInventory.h"
#include <array>
#include <cstring>
#include <istream>
#include <stdexcept>
#include <cstddef>

namespace Cache {
    inline constexpr size_t LEGACY_INVENTORY_SIZE = 360;
    inline constexpr size_t LEGACY_ITEM_PREFIX_SIZE = 84;
    inline constexpr size_t LEGACY_RING_INVENTORY_SIZE = 352;
    inline constexpr size_t LEGACY_EMPTY_SEED_SIZE = 2404;
    inline constexpr size_t EQUIPMENT_INVENTORY_SIZE = 368;
    inline constexpr uint16_t LEGACY_INVENTORY_WAL_TYPE = 1;
    inline constexpr uint16_t EQUIPMENT_INVENTORY_WAL_TYPE = 2;
    static_assert(offsetof(InventoryData, items) == 4 && sizeof(Core::MsgInventoryItem) == 8);
    static_assert(offsetof(InventoryData, recentEventIds) == 88);
    static_assert(offsetof(InventoryData, lastLsn) == 352);
    static_assert(offsetof(InventoryData, equipmentFormatTag) == LEGACY_INVENTORY_SIZE);
    static_assert(offsetof(InventoryData, equippedWeaponItemID) == 364);
    static_assert(sizeof(InventoryData) == EQUIPMENT_INVENTORY_SIZE);

    inline void ValidateInventoryImage(const InventoryData& data) {
        if (data.count > MAX_INVENTORY || data.head >= RING_SIZE || data.tail >= RING_SIZE
            || data.equipmentFormatTag != INVENTORY_EQUIPMENT_TAG)
            throw std::runtime_error("Invalid inventory image header.");
        uint16_t count = 0;
        bool weaponOwned = data.equippedWeaponItemID == 0;
        for (size_t i = 0; i < MAX_INVENTORY; ++i) {
            const auto& item = data.items[i];
            if (item.itemID == 0) {
                if (item.quantity != 0) throw std::runtime_error("Empty inventory slot has quantity.");
                continue;
            }
            if (item.quantity == 0 || item.slot != i) throw std::runtime_error("Invalid inventory item slot.");
            // Slot identity is unique; multiple existing stacks may share an item ID.
            ++count;
            if (item.itemID == data.equippedWeaponItemID) weaponOwned = true;
        }
        if (count != data.count || !weaponOwned || (data.equippedWeaponItemID != 0 && data.equippedWeaponItemID != 3))
            throw std::runtime_error("Invalid inventory count or equipped weapon.");
    }

    inline InventoryData DecodeInventory(const uint8_t* bytes, size_t size) {
        // createCharacter.sql historically seeded an entirely zero 2404-byte inventory.
        // Accept only that exact sentinel, never guess a larger slot layout or truncate items.
        if (bytes != nullptr && size == LEGACY_EMPTY_SEED_SIZE) {
            for (size_t i = 0; i < size; ++i)
                if (bytes[i] != 0) throw std::runtime_error("Non-empty legacy seed inventory is unsupported.");
            return EMPTY_INVENTORY;
        }
        if (bytes == nullptr || (size != LEGACY_ITEM_PREFIX_SIZE && size != LEGACY_RING_INVENTORY_SIZE
            && size != LEGACY_INVENTORY_SIZE && size != EQUIPMENT_INVENTORY_SIZE))
            throw std::runtime_error("Unsupported inventory image size.");
        // Check the bool representation before copying untrusted persistent bytes into a bool.
        if (size >= LEGACY_RING_INVENTORY_SIZE && bytes[offsetof(InventoryData, lastOp)] > 1)
            throw std::runtime_error("Invalid inventory ring operation.");
        InventoryData data = EMPTY_INVENTORY;
        std::memcpy(&data, bytes, size);
        ValidateInventoryImage(data);
        return data;
    }

    inline InventoryData ReadInventory(std::istream& stream) {
        std::array<uint8_t, LEGACY_EMPTY_SEED_SIZE + 1> bytes{};
        stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        if (stream.bad()) throw std::runtime_error("Inventory image read failed.");
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
        if (data.equippedWeaponItemID != 0) throw std::runtime_error("Unequip before legacy conversion.");
        std::vector<uint8_t> bytes(LEGACY_INVENTORY_SIZE);
        std::memcpy(bytes.data(), &data, bytes.size());
        return bytes;
    }

    inline std::vector<uint8_t> EncodeInventoryWal(uint64_t characterID, const InventoryData& data) {
        if (characterID == 0) throw std::runtime_error("Invalid inventory WAL character.");
        auto image = EncodeInventory(data);
        std::vector<uint8_t> payload(sizeof(characterID) + image.size());
        std::memcpy(payload.data(), &characterID, sizeof(characterID));
        std::memcpy(payload.data() + sizeof(characterID), image.data(), image.size());
        return payload;
    }

    inline InventoryData DecodeInventoryWal(uint16_t type, const uint8_t* payload, size_t size, uint64_t& characterID) {
        const size_t imageSize = type == LEGACY_INVENTORY_WAL_TYPE ? LEGACY_INVENTORY_SIZE
            : type == EQUIPMENT_INVENTORY_WAL_TYPE ? EQUIPMENT_INVENTORY_SIZE : 0;
        if (payload == nullptr || imageSize == 0 || size != sizeof(characterID) + imageSize)
            throw std::runtime_error("Unsupported inventory WAL format.");
        std::memcpy(&characterID, payload, sizeof(characterID));
        if (characterID == 0) throw std::runtime_error("Invalid inventory WAL character.");
        return DecodeInventory(payload + sizeof(characterID), imageSize);
    }
}
