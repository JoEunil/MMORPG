#include "pch.h"
#include "CacheCommandSender.h"
#include "CharacterState.h"
#include "IMessageQueue.h"
#include "LoggerGlobal.h"
#include "Message.h"
#include "MessageTypes.h"

namespace Core {
    bool CacheCommandSender::IsReady() const {
        if (queue == nullptr) {
            sysLogger->LogError("cache command sender", "message queue not initialized");
            return false;
        }
        return true;
    }

    bool CacheCommandSender::EnqueueCharacterStateUpdate(uint64_t sessionID, const CharacterState& character) const {
        if (queue == nullptr)
            return false;
        Message msg(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
        auto* update = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg.GetBuffer());
        update->header = {sessionID, MSG_CHARACTER_STATE_UPDATE};
        update->body.charID = character.characterID;
        update->body.profileId = character.profileId;
        update->body.attack = character.attack;
        update->body.level = character.level;
        update->body.exp = character.exp;
        update->body.hp = character.hp;
        update->body.mp = character.mp;
        update->body.maxHp = character.maxHp;
        update->body.maxMp = character.maxMp;
        update->body.dir = character.dir;
        update->body.x = character.x;
        update->body.y = character.y;
        update->body.lastZone = static_cast<uint8_t>(character.lastZone);
        msg.SetLength(sizeof(*update));
        return queue->EnqueueMessage(&msg);
    }

    bool CacheCommandSender::EnqueueGoldDeposit(uint64_t sessionID, uint64_t characterID, uint64_t gold) const {
        if (queue == nullptr)
            return false;
        Message msg(sizeof(MsgStruct<MsgCurrencyDepositBody>));
        auto* deposit = reinterpret_cast<MsgStruct<MsgCurrencyDepositBody>*>(msg.GetBuffer());
        deposit->header = {sessionID, MSG_CURRENCY_DEPOSIT};
        deposit->body = {characterID, gold};
        msg.SetLength(sizeof(*deposit));
        return queue->EnqueueMessage(&msg);
    }
}
