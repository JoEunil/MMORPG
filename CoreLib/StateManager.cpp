#include "pch.h"
#include "StateManager.h"
#include "Initializer.h"
#include "LobbyZone.h"
#include <iostream>

namespace Core {
    void StateManager::PingFunc() {
        m_running.store(true);
        auto packet = packetPool->Acquire();
        auto st_packet = reinterpret_cast<PacketStruct<Ping>*>(packet->GetBuffer());
        st_packet->header.magic = MAGIC;
        st_packet->header.length = sizeof(PacketStruct<Ping>);
        packet->SetLength(st_packet->header.length);
        st_packet->header.opcode = OP::PING;
        st_packet->header.flags = 0x00;
        while(m_running.load()) {
            for (int i = 0 ; i< SHARD_SIZE; i++)
            {
                auto& shard = m_shards[i];
                std::unique_lock<std::shared_mutex> lock(shard.smutex);
                for (auto it = shard.sessionMap.begin(); it != shard.sessionMap.end();) 
                {
                    std::cout << ".";
                    auto session = it->first;
                    if (it->second.pingFailCnt > 5) {
                        auto session = it->first;
                        auto sessionData = it->second;
                        it = shard.sessionMap.erase(it);

                        if (sessionData.zoneID == 0) {
                            lock.unlock();
                            lobbyZone->Disconnect(session);
                            std::cout << "Zone == lobbyZone " << std::to_string(session) << std::endl;
                            lock.lock();
                            continue;
                        }
                        CharacterState temp;
                        std::cout << "Ping failed Disconnect zone: " << std::to_string(sessionData.zoneID) << "session: " << session << std::endl;
                        m_states[sessionData.zoneID]->EmigrateChar(session, temp);
                        EnqueueDisconnectMsg(temp, session);

                    } else {
                        it++;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(3));
                
        }
    }

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
