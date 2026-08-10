#include "pch.h"
#include "StateManager.h"
#include "Initializer.h"
#include "ChatThreadPool.h"

namespace Core {
    void StateManager::CleanUp() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;
        for (auto& shard : m_shards)
        {
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            for (auto& [session, data] : shard.sessionMap)
            {
                CharacterState temp;
                if (data.zoneID == 0) {
                    if (lobbyZone->EmigrateChar(session, temp) == false) {
                        continue;
                    }
                }
                else {
                    auto it_state = m_states.find(data.zoneID);
                    if (it_state == m_states.end()) {
                        gameLogger->LogInfo("state manager", "state not found", "sessionID", session, "zoneID", data.zoneID);
                        continue;
                    }
                    if (it_state->second->EmigrateChar(session, temp) == false) {
                        continue;
                    }
                }
                Message* msg = messagePool->Acquire();
                constexpr int MAX_RETRY = 10;
                for (int retry = 0; retry < MAX_RETRY && !msg; retry++) {
                    msg = messagePool->Acquire();
                    if (!msg)
                        std::this_thread::yield();
                }

                if (!msg) {
                    std::vector<std::byte> binary(sizeof(CharacterState));
                    std::memcpy(binary.data(), &temp, sizeof(CharacterState));
                    Core::errorLogger->LogError("state manager", "failed to acquire message for disconnect", "sessionID", session, "character state", binary);
                    continue;
                }
                auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg->GetBuffer());

                st->header.sessionID = session;
                st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
                st->body.charID = temp.characterID;
                st->body.profileId = temp.profileId;
                st->body.attack = temp.attack;
                st->body.level = temp.level;
                st->body.exp = temp.exp;
                st->body.hp = temp.hp;
                st->body.mp = temp.mp;
                st->body.dir = temp.dir;
                st->body.x = temp.x;
                st->body.y = temp.y;
                st->body.lastZone = temp.lastZone;
                mq->EnqueueMessage(msg);
            }
        }
    }

    void StateManager::Disconnect(uint64_t sessionID) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock<std::shared_mutex> lock(shard.smutex);
        auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end())
        {
            return;
        }
        if (it->second.zoneID == 0) {
            lobbyZone->Disconnect(sessionID);
            shard.sessionMap.erase(it);
            return;
        }
        CharacterState temp;
        auto it_state = m_states.find(it->second.zoneID);
        if (it_state == m_states.end()) {
            gameLogger->LogInfo("state manager", "state not found", "sessionID", sessionID, "zoneID", it->second.zoneID);
            return;
        }
        if (it_state->second->EmigrateChar(sessionID, temp)) {
            auto zoneID = it->second.zoneID;
            shard.sessionMap.erase(it);
            lock.unlock();
            chat->EnqueueZoneLeave(sessionID, zoneID);
            chat->DeleteChatSession(sessionID, zoneID);
            EnqueueDisconnectMsg(temp, sessionID);
        }
    }
    // rename이 DB에 확정된 뒤, 세션이 속한 zone의 CharacterState에 새 version을 반영한다.
    // zone에 이벤트 큐를 붙여 처리하는 편이 깔끔하지만, zone 간 참조 방식을 아직 정하지 못해 여기서는 ZoneState의 기존 m_mutex를 그대로 쓴다.
    void StateManager::UpdateProfileVersion(uint64_t sessionID, uint32_t version) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock<std::shared_mutex> lock(shard.smutex);
        auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end())
            return;
        uint16_t zoneID = it->second.zoneID;
        lock.unlock();

        if (zoneID == 0) // 로비에는 전파할 대상이 없다. 진입 시 full snapshot으로 최신값이 나간다.
            return;
        auto it_state = m_states.find(zoneID);
        if (it_state == m_states.end()) {
            gameLogger->LogInfo("state manager", "state not found", "sessionID", sessionID, "zoneID", zoneID);
            return;
        }
        it_state->second->UpdateProfileVersion(sessionID, version);
    }

    void StateManager::EnqueueDisconnectMsg(CharacterState& temp, uint64_t sessionID)
    {
        gameLogger->LogInfo("state manager", "Enqueue Disconnect", "sessionID", sessionID);
        Message* msg = messagePool->Acquire();
        constexpr int MAX_RETRY = 10;
        for (int retry = 0; retry < MAX_RETRY && !msg; retry++) {
            msg = messagePool->Acquire();
            if (!msg)
                std::this_thread::yield();
        }

        if (!msg) {
            std::vector<std::byte> binary(sizeof(CharacterState));
            std::memcpy(binary.data(), &temp, sizeof(CharacterState));
            gameLogger->LogError("state manager", "Failed to acquire message for disconnect", "sessionID", sessionID, "character state", binary);
            return ;
        }

        auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg->GetBuffer());
        st->header.sessionID = sessionID;
        st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
        st->body.charID = temp.characterID;
        st->body.profileId = temp.profileId;
        st->body.level = temp.level;
        st->body.exp = temp.exp;
        st->body.hp = temp.hp;
        st->body.mp = temp.mp;
        st->body.maxHp = temp.maxHp;
        st->body.maxMp = temp.maxMp;
        st->body.dir = temp.dir;
        st->body.x = temp.x;
        st->body.y = temp.y;
        st->body.lastZone = temp.lastZone;
        msg->SetLength(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
        mq->EnqueueMessage(msg);
        messagePool->Return(msg);
    }
}
