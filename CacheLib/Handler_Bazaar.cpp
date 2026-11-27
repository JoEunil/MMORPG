#include "pch.h"
#include "Handler.h"
#include "BazaarHandler.h"

namespace Cache {
    void Handler::BazaarMyList(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarMyListBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarMyListBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->MyList(sessionID, request);
    }

    void Handler::BazaarSearch(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarSearchBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarSearchBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->Search(sessionID, request);
    }

    void Handler::BazaarRegister(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarRegisterBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarRegisterBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->Register(sessionID, request);
    }

    void Handler::BazaarCancel(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCancelBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->Cancel(sessionID, request);
    }

    void Handler::BazaarBuy(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarBuyBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarBuyBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->Buy(sessionID, request);
    }

    void Handler::BazaarCancelListing(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelListingBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCancelListingBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->CancelListing(sessionID, request);
    }

    void Handler::BazaarClaim(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarClaimBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarClaimBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->Claim(sessionID, request);
    }

    void Handler::BazaarOutboxList(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarOutboxListBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarOutboxListBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->OutboxList(sessionID, request);
    }

    void Handler::BazaarCheckOutbox(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCheckOutboxBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCheckOutboxBody>(msg.get()))
            return;
        const auto request = *body;
        msg.reset();
        bazaar->CheckOutbox(sessionID, request);
    }

}
