#include "pch.h"
#include "StateManager.h"
#include "Initializer.h"
#include "ChatThreadPool.h"
#include "WorldMap.h"
#include <algorithm>
#include <cmath>

namespace Core {
    bool StateManager::ChangeZone(uint64_t sessionID, uint8_t op, ZoneChangeResult& result) {
        result = {};
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated
            || !session->second.authCompleted || session->second.characterID == 0)
            return false;
        const uint16_t sourceID = session->second.zoneID;
        if ((op == ZONE_CHANGE::ENTER) != (sourceID == 0))
            return false;

        if (sourceID == 0) {
            std::unique_lock lobbyLock(lobbyZone->m_mutex);
            auto loaded = lobbyZone->m_chars.find(sessionID);
            if (loaded == lobbyZone->m_chars.end()
                || loaded->second.characterID != session->second.characterID)
                return false;
            const uint16_t destinationID = loaded->second.lastZone == 0 ? 1 : loaded->second.lastZone;
            auto destination = GetZone(destinationID);
            if (destination == nullptr)
                return false;
            std::unique_lock destinationLock(destination->m_mutex);
            CharacterState incoming = loaded->second;
            if (!std::isfinite(incoming.x) || !std::isfinite(incoming.y))
                return false; // Corrupt coordinates are not a map-version relocation.
            if (incoming.x < destination->m_area.x_min || incoming.x > destination->m_area.x_max
                || incoming.y < destination->m_area.y_min || incoming.y > destination->m_area.y_max
                || !WorldMap::CanTraverse(incoming.x, incoming.y, incoming.x, incoming.y)) {
                // Saved coordinates can become invalid after a fixed-map revision. Relocate only
                // on lobby entry; live Zone transfers must still prove their existing position.
                incoming.x = destination->m_area.x_min + TRANSITION_BUFFER + 10.0f;
                incoming.y = destination->m_area.y_min + TRANSITION_BUFFER + 10.0f;
                if (!WorldMap::CanTraverse(incoming.x, incoming.y, incoming.x, incoming.y)) {
                    const auto& safeArea = destination->m_cells[0][0].area;
                    const int firstX = (std::max)(0, static_cast<int>(std::floor((safeArea.x_min - WorldMap::OriginX) / WorldMap::TileSize)));
                    const int firstY = (std::max)(0, static_cast<int>(std::floor((safeArea.y_min - WorldMap::OriginY) / WorldMap::TileSize)));
                    const int lastX = (std::min)(WorldMap::Width - 1, static_cast<int>(std::floor((safeArea.x_max - WorldMap::OriginX) / WorldMap::TileSize)));
                    const int lastY = (std::min)(WorldMap::Height - 1, static_cast<int>(std::floor((safeArea.y_max - WorldMap::OriginY) / WorldMap::TileSize)));
                    bool found = false;
                    for (int y = firstY; y <= lastY && !found; ++y) {
                        for (int x = firstX; x <= lastX; ++x) {
                            const float safeX = WorldMap::OriginX + (x + 0.5f) * WorldMap::TileSize;
                            const float safeY = WorldMap::OriginY + (y + 0.5f) * WorldMap::TileSize;
                            if (safeX < safeArea.x_min || safeX > safeArea.x_max
                                || safeY < safeArea.y_min || safeY > safeArea.y_max
                                || !WorldMap::CanTraverse(safeX, safeY, safeX, safeY))
                                continue;
                            incoming.x = safeX;
                            incoming.y = safeY;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        return false; // The original lobby row and session are still intact.
                }
            }
            // Immigration copies and prepares the destination before removing the lobby row.
            if (destination->ImmigrateCharLocked(sessionID, incoming) == INVALID_ZONE_INTERNAL_ID)
                return false;
            const auto& entered = destination->m_chars.back();
            result = {destinationID, 0, entered.zoneInternalID, entered.x, entered.y, destination->m_generation};
            lobbyZone->m_chars.erase(loaded);
            session->second.zoneID = destinationID;
            result.chatID = chat->AddChatSession(sessionID, destinationID, entered.profileId);
            chat->EnqueueZoneJoin(sessionID, destinationID);
            return true;
        }

        int destinationID = sourceID;
        const int column = (sourceID - 1) % ZONE_HORIZON;
        switch (op) {
        case ZONE_CHANGE::UP: destinationID += ZONE_HORIZON; break;
        case ZONE_CHANGE::DOWN: destinationID -= ZONE_HORIZON; break;
        case ZONE_CHANGE::LEFT:
            if (column == 0) return false;
            --destinationID;
            break;
        case ZONE_CHANGE::RIGHT:
            if (column == ZONE_HORIZON - 1) return false;
            ++destinationID;
            break;
        default: return false;
        }
        if (destinationID <= 0 || destinationID > ZONE_COUNT)
            return false;
        auto source = GetZone(sourceID);
        auto destination = GetZone(static_cast<uint16_t>(destinationID));
        if (source == nullptr || destination == nullptr)
            return false;
        // All transfers use ascending Zone order after the session shard.
        std::unique_lock firstLock(sourceID < destinationID ? source->m_mutex : destination->m_mutex);
        std::unique_lock secondLock(sourceID < destinationID ? destination->m_mutex : source->m_mutex);
        auto character = source->m_sessionToIndex.find(sessionID);
        if (character == source->m_sessionToIndex.end())
            return false;
        const auto& current = source->m_chars[character->second];
        if (current.characterID != session->second.characterID
            || !std::isfinite(current.x) || !std::isfinite(current.y)
            || current.x < source->m_area.x_min || current.x > source->m_area.x_max
            || current.y < source->m_area.y_min || current.y > source->m_area.y_max)
            return false;
        if (destination->ImmigrateCharLocked(sessionID, current) == INVALID_ZONE_INTERNAL_ID)
            return false;
        source->RemoveCharacterLocked(sessionID);
        session->second.zoneID = static_cast<uint16_t>(destinationID);
        const auto& entered = destination->m_chars.back();
        result = {static_cast<uint16_t>(destinationID), 0, entered.zoneInternalID,
            entered.x, entered.y, destination->m_generation};
        chat->EnqueueZoneJoin(sessionID, static_cast<uint16_t>(destinationID));
        return true;
    }

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
                Message msg(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
                auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg.GetBuffer());

                st->header.sessionID = session;
                st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
                st->body.charID = temp.characterID;
                st->body.profileId = temp.profileId;
                st->body.attack = temp.attack;
                st->body.level = temp.level;
                st->body.exp = temp.exp;
                st->body.hp = temp.hp;
                st->body.mp = temp.mp;
                st->body.maxHp = temp.maxHp;
                st->body.maxMp = temp.maxMp;
                st->body.dir = temp.dir;
                st->body.x = temp.x;
                st->body.y = temp.y;
                st->body.lastZone = static_cast<uint8_t>(temp.lastZone);
				msg.SetLength(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
                if (!mq->EnqueueMessage(&msg)) {
                    std::vector<std::byte> binary(sizeof(CharacterState));
                    std::memcpy(binary.data(), &temp, sizeof(CharacterState));
                    Core::errorLogger->LogError("state manager", "failed to enqueue message for disconnect", "sessionID", session, "character state", binary);
                }
            }
            shard.sessionMap.clear();
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
            chat->EnqueueZoneLeave(sessionID, zoneID);
            chat->DeleteChatSession(sessionID, zoneID);
            lock.unlock();
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
        Message msg(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));
        auto st = reinterpret_cast<MsgStruct<MsgCharacterStateUpdateBody>*>(msg.GetBuffer());
        st->header.sessionID = sessionID;
        st->header.messageType = MSG_CHARACTER_STATE_UPDATE;
        st->body.charID = temp.characterID;
        st->body.profileId = temp.profileId;
        st->body.attack = temp.attack;
        st->body.level = temp.level;
        st->body.exp = temp.exp;
        st->body.hp = temp.hp;
        st->body.mp = temp.mp;
        st->body.maxHp = temp.maxHp;
        st->body.maxMp = temp.maxMp;
        st->body.dir = temp.dir;
        st->body.x = temp.x;
        st->body.y = temp.y;
        st->body.lastZone = static_cast<uint8_t>(temp.lastZone);
        msg.SetLength(sizeof(MsgStruct<MsgCharacterStateUpdateBody>));

        if (!mq->EnqueueMessage(&msg)) {
            std::vector<std::byte> binary(sizeof(CharacterState));
            std::memcpy(binary.data(), &temp, sizeof(CharacterState));
            errorLogger->LogError("state manager", "Failed to enqueue disconnect message", "sessionID", sessionID, "character state", binary);
        }
    }
}
