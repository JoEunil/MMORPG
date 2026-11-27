#include "pch.h"
#include "CharacterHandler.h"
#include "CacheResponseSender.h"
#include "CacheStorageInventory.h"
#include "ProfileCache.h"
#include "DB/DBWorker.h"
#include "DB/IResultReader.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace Cache {
    Core::MsgCharacterListResBody CharacterHandler::ReadCharacterList(IResultReader* result) {
        Core::MsgCharacterListResBody response{};
        if (!result || !result->next())
            return response;
        response.resStatus = 1;
        do {
            if (response.count >= MAX_CHARACTER_CNT) break;
            auto& info = response.characters[response.count];
            info.characterID = result->getUInt64("char_id");
            const std::string name = result->getString("name");
            std::memcpy(info.name, name.c_str(), std::min(name.size(), sizeof(info.name) - 1));
            info.level = result->getUInt("level");
            ++response.count;
        } while (result->next());
        return response;
    }

    void CharacterHandler::List(uint64_t sessionID, Core::MsgCharacterListReqBody request) {

        dbWorkerGame->Enqueue([this, sessionID, request](DBConnectionGame* conn) {
            auto res = conn->ExecuteSelect(StatementId::SelectCharacterList, request.userID, request.channelID);
            const auto response = ReadCharacterList(res.get());
            if (!response.resStatus)
                Core::gameLogger->LogInfo("cache handler", "character list read failed", {{"sessionID", sessionID}, {"user_id", request.userID}, {"channel_id", request.channelID}});
            responses->Send({ sessionID, Core::MSG_CHARACTER_LIST_RES }, response);
            });
    }

    void CharacterHandler::LoadState(uint64_t sessionID, Core::MsgCharacterStateReqBody request) {

        auto completed = std::make_shared<std::atomic_bool>(false);
        const auto finish = [this, sessionID, completed](const Core::MsgCharacterStateResBody& response) {
            if (completed->exchange(true))
                return;
            if (responses->Send({ sessionID, Core::MSG_CHARACTER_STATE_RES }, response) != CacheResponseSender::Result::Queued)
                Core::errorLogger->LogError("cache handler", "character state response queue rejected", {{"sessionID", sessionID}});
        };
        const auto failed = [finish] { finish(Core::MsgCharacterStateResBody{}); };
        const auto outcome = dbWorkerGame->Enqueue([this, request, finish](DBConnectionGame* conn) {
            auto res = conn->ExecuteSelect(StatementId::SelectCharacterState, request.characterID, request.userID, request.channelID);
            if (!res || !res->next()) { finish(Core::MsgCharacterStateResBody{}); return; }
            Core::MsgCharacterStateResBody response{};
            response.charID = res->getUInt64("char_id");
            response.resStatus = 1;
            const std::string name = res->getString("name");
            response.profileId = res->getUInt("profile_id");
            response.profileVersion = res->getUInt("profile_version");
            std::memcpy(response.name, name.c_str(), std::min(name.size(), sizeof(response.name) - 1));
            cache_profile->Load(response.profileId, response.profileVersion, name);
            response.attack = res->getUInt("attack");
            response.level = res->getUInt("level");
            response.exp = res->getInt64("exp");
            response.hp = res->getInt("hp");
            response.mp = res->getInt("mp");
            response.maxHp = res->getInt("max_hp");
            response.maxMp = res->getInt("max_mp");
            response.dir = res->getInt("dir");
            response.currentZone = res->getUInt("zone_id");
            response.startX = res->getDouble("last_pos_x");
            response.startY = res->getDouble("last_pos_y");
            cache_inventory->LoadEquipmentState(response.charID, [finish, response](Core::EquipmentStateResult equipment) mutable {
                if (equipment.status != Core::EquipmentOperationStatus::Success
                    || (equipment.weaponID != 0 && equipment.weaponID != 3)
                    || (equipment.shieldID != 0 && equipment.shieldID != 4)
                    || (equipment.bodyID != 0 && equipment.bodyID != 5)) {
                    finish(Core::MsgCharacterStateResBody{}); return;
                }
                response.equippedWeaponItemID = equipment.weaponID;
                response.equippedShieldItemID = equipment.shieldID;
                response.equippedBodyItemID = equipment.bodyID;
                finish(response); // Never publish a playable character before equipment is hydrated.
            });
        }, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
    }

    // 접속 종료 경로. 캐릭터 상태를 DB에 쓰면서 profile 캐시도 함께 내린다.
    // write-through라 flush를 기다릴 필요 없이 바로 지워도 유실이 없다.
    void CharacterHandler::SaveState(Core::ScopedMessage msg) {
        const auto* header = Core::parseMsgHeader(msg->GetBuffer());
        const auto sessionID = header->sessionID;
        const auto* body = Core::parseMsgBody<Core::MsgCharacterStateUpdateBody>(msg->GetBuffer());

        cache_profile->Unload(body->profileId);
        const auto state = *body;
        const auto outcome = dbWorkerGame->Enqueue([state, sessionID](DBConnectionGame* conn) {
            auto res = conn->ExecuteUpdate(StatementId::UpdateCharacterState, state.attack, state.level, state.exp, state.hp, state.mp, state.maxHp, state.maxMp, state.dir, state.x, state.y, state.lastZone, state.charID);

            if (!res) {
                Core::gameLogger->LogInfo("cache handler", "character state update failed", {{"sessionID", sessionID}, {"char_id", state.charID}, {"level", state.level},
                    {"exp", state.exp}, {"hp", state.hp}, {"mp", state.mp}, {"max_hp", state.maxHp}, {"max_mp", state.maxMp}, {"dir", state.dir}, {"x", state.x}, {"y", state.y}, {"last_zone", state.lastZone}});
            }
        });
        if (outcome != DBEnqueueResult::Queued) {
            const auto* header = Core::parseMsgHeader(msg->GetBuffer());
            std::vector<std::byte> binary(msg->GetLength());
            std::memcpy(binary.data(), msg->GetBuffer(), msg->GetLength());
            Core::errorLogger->LogError("cache handler", "character state update queue rejected",
                {{"sessionID", header->sessionID}, {"messageType", header->messageType}, {"message", binary}});
        }
    }

    // 쓰기는 이 한 경로로만 들어온다. Rename 내부에서 profile_id당 in-flight를 1개로 제한하므로(ProfileCache.h 참고) 
    // DBWorker가 스레드풀이라도 캐시 갱신 순서가 뒤집히지 않는다.
  
    void CharacterHandler::RenameProfile(uint64_t sessionID, Core::MsgProfileRenameBody request) {

        uint32_t profileId = request.profileId;
        std::string name(reinterpret_cast<const char*>(request.name),
            strnlen(reinterpret_cast<const char*>(request.name), Core::MAX_CHARNAME_LEN));

        cache_profile->Rename(profileId, name, [this, sessionID, profileId](bool success, uint32_t version) {
            if (!success) {
                Core::gameLogger->LogInfo("cache handler", "profile rename failed",
                    {{"sessionID", sessionID}, {"profile_id", profileId}});
            }
            const Core::MsgProfileRenameResBody response{ static_cast<uint8_t>(success), profileId, version };
            const auto result = responses->Send({ sessionID, Core::MSG_PROFILE_RENAME_RES }, response);
            if (result == CacheResponseSender::Result::PoolExhausted) {
                Core::errorLogger->LogError("cache handler", "failed to acquire message for rename response",
                    {{"sessionID", sessionID}, {"profile_id", profileId}, {"success", success}});
                return;
            }
            });
    }
}
