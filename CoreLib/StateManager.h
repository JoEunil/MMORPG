#pragma once

#include <vector>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <array>
#include <chrono>

#include "IMessageQueue.h"
#include "MessageTypes.h"
#include "MessagePool.h"

#include "ZoneState.h"
#include "Config.h"
#include "IPacket.h"
#include "IIOCP.h"
#include "IPacketPool.h"
#include "PacketTypes.h"
#include <iostream>

namespace Core {
    struct SessionData {
        uint16_t zoneID = 0;
        uint64_t userID = 0;
        uint64_t characterID = 0;
        uint16_t cheatCount = 0;
        //uint64_t guildID = 0;  
        //uint64_t partyID = 0; 
        // session의 guild, party는 여기서 1차로 관리. 
        std::chrono::steady_clock::time_point lastCheatTime;
        bool authenticated = false;
    };

    struct SessionShard {
        std::shared_mutex smutex;
        std::unordered_map<uint64_t, SessionData> sessionMap;
    };
    class LobbyZone;
    class ChatThreadPool;
    class StateManager {
        std::unordered_map<uint16_t, std::unique_ptr<Core::ZoneState>> m_states;
        std::array<SessionShard, SHARD_SIZE> m_shards; // mutex있어서 vector 사용 불가
        
        
        MessagePool* messagePool;
        IMessageQueue* mq;
        IIOCP* iocp;
        IPacketPool* packetPool;
        LobbyZone* lobbyZone;
        ChatThreadPool* chat;

        void Initialize(IMessageQueue* m, IIOCP* io, MessagePool* mp, IPacketPool* pp, LobbyZone* lobby, ChatThreadPool* c) {
            m_states.reserve(ZONE_COUNT);
            for (int zoneID = 1; zoneID <= ZONE_COUNT; zoneID++) {
                m_states.emplace(zoneID, std::make_unique<ZoneState>(zoneID));
                m_states[zoneID]->InitializeMonster();
            }
            for (auto& shard: m_shards)
                shard.sessionMap.reserve(MAX_USER_CAPACITY / SHARD_SIZE);
            mq = m;
            messagePool = mp;
            lobbyZone = lobby;
            packetPool = pp;
            iocp = io;
            chat = c;
        }
        
        bool IsReady() {
            if (mq == nullptr) return false;
            if (messagePool == nullptr) return false;
            if (lobbyZone == nullptr) return false;
            if (chat == nullptr) return false;
            return true;
        }
        
        
        void CleanUp() {
            for (auto& shard : m_shards)
            {
                std::unique_lock<std::shared_mutex> lock(shard.smutex);
                for (auto& [session, data] : shard.sessionMap)
                {
                    CharacterState temp;
                    m_states[data.zoneID]->EmigrateChar(session, temp);
                    Message* msg = messagePool->Acquire();
                    auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg);

                    st->header.sessionID = session;
                    st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
                    st->body.charID = temp.characterID;
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

        friend class Initializer;

    public:
        ~StateManager() {
            CleanUp();
        }

        ZoneState* GetZone(uint16_t zoneID) {
            auto it = m_states.find(zoneID);
            return it != m_states.end() ? it->second.get() : nullptr;
        }

        void AddSession(uint64_t sessionID, uint64_t userID, bool authenticated) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            auto& map = shard.sessionMap;
            auto it = map.find(sessionID);
            if (it != map.end())
                return ;
            map[sessionID].userID = userID;
            map[sessionID].zoneID = 0; // lobby
            map[sessionID].characterID = 0;
            map[sessionID].cheatCount = 0;
            map[sessionID].lastCheatTime = std::chrono::steady_clock::now();
            map[sessionID].authenticated = authenticated;
            // character load 안한 상태
        }
        
        uint64_t GetUserID(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return 0;
            return it->second.userID;
        }

        void SetCharacterID(uint64_t sessionID, uint64_t characterID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return ;
            it->second.characterID = characterID;
        }
        
        uint64_t GetCharacterID(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return 0;
            return it->second.characterID;
        }
        
        void SetZoneID(uint64_t sessionID, uint16_t dest) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
                return ;
            it->second.zoneID = dest;
        }
        
        int16_t GetZoneID(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it == shard.sessionMap.end())
            {
                return -1;
            }
            return it->second.zoneID;
        }

        uint8_t HealthCheck(uint64_t sessionID) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::shared_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            uint8_t res = 0; 
            if (it == shard.sessionMap.end())
            {
                return res;
            }
            res |= MASK_EXIST;
            if (it->second.authenticated)
                res |= MASK_AUTHENTICATED; 
            if (it->second.cheatCount <= MAX_CHEAT_COUNT)
                res |= MASK_NOT_CHEAT; 
            return res;
        }
        void Disconnect(uint64_t sessionID);
        void EnqueueDisconnectMsg(CharacterState& temp, uint64_t sessionID);

        void Cheat(uint64_t sessionID, uint16_t cheat, std::chrono::steady_clock::time_point time) {
            auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> lock(shard.smutex);
            auto it = shard.sessionMap.find(sessionID);
            if (it != shard.sessionMap.end())
            {
                auto before = it->second.lastCheatTime;
                it->second.lastCheatTime = time;
                if (it->second.lastCheatTime - before > CHEAT_FLUSH_TIME) {
                    it->second.cheatCount = 0;
                }
                it->second.cheatCount += cheat;
            }
        }
    };
}
