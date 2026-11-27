#include "pch.h"
#include "MessageDispatcher.h"
#include "BazaarHandler.h"
#include "CurrencyHandler.h"
#include "DiamondHandler.h"
#include "InventoryHandler.h"
#include "CharacterHandler.h"

#include <CoreLib/Interface/IMessage.h>
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>


namespace Cache {
    void MessageDispatcher::Process(ScopedMessage msg) {
        if (!msg || msg->GetLength() < sizeof(Core::MsgHeader)) {
            return;
        }

        Core::MsgHeader* header = Core::parseMsgHeader(msg->GetBuffer());
        const auto sessionID = header->sessionID;
        switch (header->messageType) {
        case Core::MSG_CHARACTER_LIST_REQ :
            if (const auto request = ReadRequest<Core::MsgCharacterListReqBody>(msg))
                character->List(sessionID, *request);
            break;
        case Core::MSG_CHARACTER_STATE_REQ:
            if (const auto request = ReadRequest<Core::MsgCharacterStateReqBody>(msg))
                character->LoadState(sessionID, *request);
            break;
        case Core::MSG_CHARACTER_STATE_UPDATE:
            if (CheckMessageLength<Core::MsgCharacterStateUpdateBody>(msg.get()))
                character->SaveState(std::move(msg));
            break;
        case Core::MSG_INVENTORY_REQ:
            if (const auto request = ReadRequest<Core::MsgInventoryReqBody>(msg))
                inventory->Request(sessionID, *request);
            break;
        case Core::MSG_INVENTORY_UPDATE:
            if (const auto request = ReadRequest<Core::MsgInventoryUpdateBody>(msg))
                inventory->Update(sessionID, *request);
            break;
        case Core::MSG_CURRENCY_REQ:
            if (const auto request = ReadRequest<Core::MsgCurrencyReqBody>(msg))
                currency->Request(sessionID, *request);
            break;
        case Core::MSG_CURRENCY_DEPOSIT:
            if (const auto request = ReadRequest<Core::MsgCurrencyDepositBody>(msg))
                currency->Deposit(sessionID, *request);
            break;
        case Core::MSG_DIAMOND_REQ:
            if (const auto request = ReadRequest<Core::MsgDiamondReqBody>(msg))
                diamond->Request(sessionID, *request);
            break;
        case Core::MSG_DIAMOND_DEPOSIT:
            if (const auto request = ReadRequest<Core::MsgDiamondDepositBody>(msg))
                diamond->Deposit(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_MY_LIST:
            if (const auto request = ReadRequest<Core::MsgBazaarMyListBody>(msg))
                bazaar->MyList(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_SEARCH:
            if (const auto request = ReadRequest<Core::MsgBazaarSearchBody>(msg))
                bazaar->Search(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_REGISTER:
            if (const auto request = ReadRequest<Core::MsgBazaarRegisterBody>(msg))
                bazaar->Register(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_CANCEL:
            if (const auto request = ReadRequest<Core::MsgBazaarCancelBody>(msg))
                bazaar->Cancel(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_BUY:
            if (const auto request = ReadRequest<Core::MsgBazaarBuyBody>(msg))
                bazaar->Buy(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_CLAIM:
            if (const auto request = ReadRequest<Core::MsgBazaarClaimBody>(msg))
                bazaar->Claim(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_OUTBOX_LIST:
            if (const auto request = ReadRequest<Core::MsgBazaarOutboxListBody>(msg))
                bazaar->OutboxList(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_CHECK_OUTBOX:
            if (const auto request = ReadRequest<Core::MsgBazaarCheckOutboxBody>(msg))
                bazaar->CheckOutbox(sessionID, *request);
            break;
        case Core::MSG_BAZAAR_CANCEL_LISTING:
            if (const auto request = ReadRequest<Core::MsgBazaarCancelListingBody>(msg))
                bazaar->CancelListing(sessionID, *request);
            break;
        case Core::MSG_PROFILE_RENAME:
            if (const auto request = ReadRequest<Core::MsgProfileRenameBody>(msg))
                character->RenameProfile(sessionID, *request);
            break;
        }
    }
}
