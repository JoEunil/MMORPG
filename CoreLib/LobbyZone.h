#pragma once
#include "Config.h"
#include "ZoneState.h"
#include "ILogger.h"
#include "StateManager.h"

namespace Core {
    class LobbyZone {
        // 월드 진입 전 db 비동기 결과 보관.
        std::unordered_map<uint64_t, CharacterState> m_chars;
        std::mutex m_mutex;
        
        ILogger* logger;
        StateManager* stateManager;
        void Initialize(ILogger* l, StateManager* s) {
            m_chars.reserve(MAX_USER_CAPACITY);
            logger = l;
            stateManager = s;
        }
        bool IsReady() {
            if (logger == nullptr)
                return false;
            return true;
        }
        friend class Initializer;
    public:
        void Disconnect(uint64_t sessionID) {
            // db에서 load 후 변경되지 않은 상태
            std::lock_guard lock(m_mutex);
            auto it = m_chars.find(sessionID);
            if (it != m_chars.end()) {
                logger->LogWarn(std::format("Character State not exist in LobbyZone, session: {}", sessionID));
                return;
            }
            m_chars.erase(it);
        }
        
        bool ImmigrateChar(uint64_t sessionID, CharacterState& c) {
            std::lock_guard lock(m_mutex);
            auto it = m_chars.find(sessionID);
            if (it != m_chars.end()) {
                logger->LogError(std::format("ImmigrageChar Failed sessionID already exists in lobby zone! sessionID={}", sessionID));
                return false;
            }
            m_chars[sessionID] = c;
            stateManager->SetZoneID(sessionID, 0);
            return true;
        }
        
        bool EmigrateChar(uint64_t sessionID, CharacterState& o){
            auto it = m_chars.find(sessionID);
            if (it == m_chars.end()) {
                logger->LogError(std::format("EmigrageChar Failed sessionID not exists in lobby zone! sessionID={}", sessionID));
                return false;
            }
            o = m_chars[sessionID];
            return true;
        }
        
    };

}
