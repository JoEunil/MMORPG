#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "Config.h"
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/Interface/IMessagePool.h>

#include <CoreLib/LoggerGlobal.h>
namespace Core {
    class ILogger;
    class IMessage;
}

namespace Cache {
    class BazaarHandler;
    class CurrencyHandler;
    class DiamondHandler;
    class InventoryHandler;
    class CharacterHandler;

    class MessageDispatcher {
        using ScopedMessage = Core::ScopedMessage;
        BazaarHandler* bazaar = nullptr;
        CurrencyHandler* currency = nullptr;
        DiamondHandler* diamond = nullptr;
        InventoryHandler* inventory = nullptr;
        CharacterHandler* character = nullptr;

        template <typename T>
        bool CheckMessageLength(const Core::IMessage* msg) {
            return msg->GetLength() == sizeof(Core::MsgStruct<T>);
        }

        template<typename Body>
        std::optional<Body> ReadRequest(ScopedMessage& msg) {
            if (!msg || !CheckMessageLength<Body>(msg.get()))
                return std::nullopt;
            const auto request = *Core::parseMsgBody<Body>(msg->GetBuffer());
            msg.reset();
            return request;
        }

        void Initialize(BazaarHandler* bazaarHandler, CurrencyHandler* currencyHandler,
            DiamondHandler* diamondHandler, InventoryHandler* inventoryHandler, CharacterHandler* characterHandler) {
            bazaar = bazaarHandler;
            currency = currencyHandler;
            diamond = diamondHandler;
            inventory = inventoryHandler;
            character = characterHandler;
        }
        bool IsReady() const {
            if (bazaar == nullptr) {
                Core::sysLogger->LogError("cache dispatcher", "bazaar handler not initialized");
                return false;
            }
            if (currency == nullptr) {
                Core::sysLogger->LogError("cache dispatcher", "currency handler not initialized");
                return false;
            }
            if (diamond == nullptr) {
                Core::sysLogger->LogError("cache dispatcher", "diamond handler not initialized");
                return false;
            }
            if (inventory == nullptr) {
                Core::sysLogger->LogError("cache dispatcher", "inventory handler not initialized");
                return false;
            }
            if (character == nullptr) {
                Core::sysLogger->LogError("cache dispatcher", "character handler not initialized");
                return false;
            }
            return true;
        }

        friend class Initializer;
        friend class BazaarSearchFixture;
        friend class BazaarMyListFixture;
    public:
        void Process(ScopedMessage msg);
    };
}
