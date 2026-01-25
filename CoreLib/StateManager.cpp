#include "pch.h"
#include "StateManager.h"
#include "Initializer.h"
#include "LobbyZone.h"
#include "ChatThreadPool.h"
#include <iostream>

namespace Core {
    void StateManager::Disconnect(uint64_t sessionID) {
        auto& shard = m_shards[sessionID % SHARD_SIZE];
        std::unique_lock<std::shared_mutex> lock(shard.smutex);
        auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end())
        {
            std::cout << "Session already Disconnected " << std::to_string(sessionID) << std::endl;
            return;
        }
        if (it->second.zoneID == 0) {
            lobbyZone->Disconnect(sessionID);
            std::cout << "Zone == lobbyZone " << std::to_string(sessionID) << std::endl;
            return;
        }
        CharacterState temp;
        std::cout << " Disconnect zone: " << std::to_string(it->second.zoneID) << "session: " << sessionID << std::endl;
        if (m_states[it->second.zoneID]->EmigrateChar(sessionID, temp)) {
            shard.sessionMap.erase(it);
            lock.unlock();
            chat->EnqueueZoneLeave(sessionID, it->second.zoneID);
            chat->DeleteChatSession(sessionID, it->second.zoneID);
            EnqueueDisconnectMsg(temp, sessionID);
        }
    }
    void StateManager::EnqueueDisconnectMsg(CharacterState& temp, uint64_t sessionID)
    {
        Message* msg = messagePool->Acquire();
        auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg->GetBuffer());
        st->header.sessionID = sessionID;
        st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
        st->body.charID = temp.characterID;
        st->body.level = temp.level;
        st->body.exp = temp.exp;
        st->body.hp = temp.hp;
        st->body.mp = temp.mp;
        st->body.dir = temp.dir;
        st->body.x = temp.x;
        st->body.y = temp.y;
        st->body.lastZone = temp.lastZone;
        msg->SetLength(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
        mq->EnqueueMessage(msg);
        messagePool->Return(msg);
    }
}
