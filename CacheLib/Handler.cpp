#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"
#include "MessagePool.h"

namespace Cache {
    void Handler::Process(Core::Message* msg) {
        Core::MsgHeader* header = Core::parseMsgHeader(msg->GetBuffer());
        switch (header->messageType) {
        case Core::MSG_CHARACTER_LIST_REQ :
                CharacterListRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgCharacterListReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_CHARACTER_STATE_REQ:
            CharacterStateRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgCharacterStateReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_CHARACTER_STATE_UPDATE:
            CharacterStateUpdate(msg, header->sessionID, Core::parseMsgBody<Core::MsgCharacterStateUpdateBody>(msg->GetBuffer()));
            break;
        case Core::MSG_INVENTORY_REQ:
            InventoryRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgInventoryReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_INVENTORY_UPDATE:
            InventoryUpdate(msg, header->sessionID, Core::parseMsgBody<Core::MsgInventoryUpdateBody>(msg->GetBuffer()));
            break;
        case Core::MSG_CURRENCY_REQ:
            CurrencyRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgCurrencyReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_CURRENCY_DEPOSIT:
            CurrencyDeposit(msg, header->sessionID, Core::parseMsgBody<Core::MsgCurrencyDepositBody>(msg->GetBuffer()));
            break;
        case Core::MSG_DIAMOND_REQ:
            DiamondRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgDiamondReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_DIAMOND_DEPOSIT:
            DiamondDeposit(msg, header->sessionID, Core::parseMsgBody<Core::MsgDiamondDepositBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_MY_LIST:
            BazaarMyList(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarMyListBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_SEARCH:
            BazaarSearch(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarSearchBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_REGISTER:
            if (msg->GetLength() != sizeof(Core::MsgStruct<Core::MsgBazaarRegisterBody>)) break;
            BazaarRegister(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarRegisterBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_CANCEL:
            BazaarCancel(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarCancelBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_BUY:
            if (msg->GetLength() != sizeof(Core::MsgStruct<Core::MsgBazaarBuyBody>)) break;
            BazaarBuy(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarBuyBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_CLAIM:
            if (msg->GetLength() != sizeof(Core::MsgStruct<Core::MsgBazaarClaimBody>))
                break;
            BazaarClaim(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarClaimBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_CHECK_OUTBOX:
            if (msg->GetLength() != sizeof(Core::MsgStruct<Core::MsgBazaarCheckOutboxBody>)) break;
            BazaarCheckOutbox(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarCheckOutboxBody>(msg->GetBuffer()));
            break;
        case Core::MSG_BAZAAR_CANCEL_LISTING:
            if (msg->GetLength() != sizeof(Core::MsgStruct<Core::MsgBazaarCancelListingBody>)) break;
            BazaarCancelListing(msg, header->sessionID, Core::parseMsgBody<Core::MsgBazaarCancelListingBody>(msg->GetBuffer()));
            break;
        case Core::MSG_PROFILE_RENAME:
            ProfileRename(msg, header->sessionID, Core::parseMsgBody<Core::MsgProfileRenameBody>(msg->GetBuffer()));
            break;
        }
        if (msg != nullptr)
            messagePool->Return(msg);
    }
    void Handler::CharacterListRequest(Core::Message*& msg, uint64_t sessionID, Core::MsgCharacterListReqBody* body) {
        EnqueueMessageWork(dbWorkerGame, msg, [=](DBConnectionGame* conn) {
            auto res = conn->ExecuteSelect(1, body->userID, body->channelID);
            Core::MsgStruct<Core::MsgCharacterListResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgCharacterListResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_CHARACTER_LIST_RES;

            if (!res || !res->next()) {
                Core::gameLogger->LogInfo("cache handler", "character list read failed", "sessionID", sessionID, "user_id", body->userID, "channel_id", body->channelID);
                st->body.resStatus = 0;
                st->body.count = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterListResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                return;
            }

            st->body.resStatus = 1;
            st->body.count = 0;

            do {
                if (st->body.count >= MAX_CHARACTER_CNT) break;

                auto& info = st->body.characters[st->body.count];
                info.characterID = res->getUInt64("char_id");
                std::string name = res->getString("name");
                std::memset(info.name, 0, sizeof(info.name));
                std::memcpy(info.name, name.c_str(), std::min(name.size(), sizeof(info.name) - 1));
                info.level = res->getUInt("level");

                st->body.count++;
            } while (res->next());

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterListResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            });
    }

    void Handler::CharacterStateRequest(Core::Message*& msg, uint64_t sessionID, Core::MsgCharacterStateReqBody* body) {
        EnqueueMessageWork(dbWorkerGame, msg, [=](DBConnectionGame* conn) {
            auto res = conn->ExecuteSelect(3, body->characterID, body->userID, body->channelID);

            Core::MsgStruct<Core::MsgCharacterStateResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgCharacterStateResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_CHARACTER_STATE_RES;

            if (!res || !res->next()) {
                Core::gameLogger->LogInfo("cache handler", "character state read failed", "sessionID", sessionID, "char_id", body->characterID);
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterStateResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                return;
            }
            st->body.charID = res->getUInt64("char_id");
            st->body.resStatus = 1;
            std::string name = res->getString("name");
            st->body.profileId = res->getUInt("profile_id");
            st->body.profileVersion = res->getUInt("profile_version");

            std::memset(st->body.name, 0, sizeof(st->body.name));
            std::memcpy(st->body.name, name.c_str(), std::min(name.size(), sizeof(st->body.name) - 1));
            st->body.name[sizeof(st->body.name) - 1] = '\0';

            // 접속 시 적재. QUERY_3에서 이미 profile을 조인해 읽었으므로 DB 왕복이 늘지 않는다.
            cache_profile->Load(st->body.profileId, st->body.profileVersion, name);

            st->body.attack = res->getUInt("attack");
            st->body.level = res->getUInt("level");
            st->body.exp = res->getInt64("exp");
            st->body.hp = res->getInt("hp");
            st->body.mp = res->getInt("mp");
            st->body.maxHp = res->getInt("max_hp");
            st->body.maxMp = res->getInt("max_mp");
            st->body.dir = res->getInt("dir");
            st->body.currentZone = res->getUInt("zone_id");
            st->body.startX = res->getDouble("last_pos_x");
            st->body.startY = res->getDouble("last_pos_y");

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterStateResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
        });
    }

    // 접속 종료 경로. 캐릭터 상태를 DB에 쓰면서 profile 캐시도 함께 내린다.
    // write-through라 flush를 기다릴 필요 없이 바로 지워도 유실이 없다.
    void Handler::CharacterStateUpdate(Core::Message*& msg, uint64_t sessionID, Core::MsgCharacterStateUpdateBody* body) {
        cache_profile->Unload(body->profileId);
        const auto state = *body;
        dbWorkerGame->Enqueue([state, sessionID](DBConnectionGame* conn) {
            auto res = conn->ExecuteUpdate(4, state.attack, state.level, state.exp, state.hp, state.mp, state.maxHp, state.maxMp, state.dir, state.x, state.y, state.lastZone, state.charID);

            if (!res) {
                Core::gameLogger->LogInfo("cache handler", "character state update failed", "sessionID", sessionID, "char_id", state.charID, "level", state.level,
                    "exp", state.exp, "hp", state.hp, "mp", state.mp, "max_hp", state.maxHp, "max_mp", state.maxMp, "dir", state.dir, "x", state.x, "y", state.y, "last_zone", state.lastZone);
            }
        }, msg);
    }

    // 쓰기는 이 한 경로로만 들어온다. Rename 내부에서 profile_id당 in-flight를 1개로 제한하므로(ProfileCache.h 참고) 
    // DBWorker가 스레드풀이라도 캐시 갱신 순서가 뒤집히지 않는다.
  
    void Handler::ProfileRename(Core::Message*& msg, uint64_t sessionID, Core::MsgProfileRenameBody* body) {
        uint32_t profileId = body->profileId;
        std::string name(reinterpret_cast<const char*>(body->name),
            strnlen(reinterpret_cast<const char*>(body->name), Core::MAX_CHARNAME_LEN));

        cache_profile->Rename(profileId, name, [this, sessionID, profileId](bool success, uint32_t version) {
            if (!success) {
                Core::gameLogger->LogInfo("cache handler", "profile rename failed",
                    "sessionID", sessionID, "profile_id", profileId);
            }
            Core::Message* res = messagePool->Acquire();
            if (res == nullptr) {
                Core::errorLogger->LogError("cache handler", "failed to acquire message for rename response",
                    "sessionID", sessionID, "profile_id", profileId, "success", success);
                return;
            }
            auto st = reinterpret_cast<Core::MsgStruct<Core::MsgProfileRenameResBody>*>(res->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_PROFILE_RENAME_RES;
            st->body.resStatus = success ? 1 : 0;
            st->body.profileId = profileId;
            st->body.version = version; // 실패 시 0
            res->SetLength(sizeof(Core::MsgStruct<Core::MsgProfileRenameResBody>));
            messageQ->EnqueueMessage(res);
            messagePool->Return(res);
            });
    }
}
