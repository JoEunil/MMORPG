#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <shared_mutex>

#include <CoreLib/IProfileCache.h>
#include <CoreLib/PacketTypes.h>

#include "DBWorker.h"
#include "DBConnectionGame.h"

namespace Cache {
    // 이름 조회용 write-through 캐시.

    struct ProfileData {
        uint32_t version;
        std::string name;
        bool renaming = false;
    };

    class ProfileCache : public Core::IProfileCache {
        DBWorker<DBConnectionGame>* dbWorker = nullptr;

        // 읽기가 대부분이라 shared_mutex. 샤딩은 프로필 쓰기 빈도를 보고 나중에 판단.
        std::unordered_map<uint32_t, ProfileData> m_profiles;
        std::shared_mutex m_mutex;

        void Initialize(DBWorker<DBConnectionGame>* d) {
            dbWorker = d;
            m_profiles.reserve(Core::MAX_USER_CAPACITY);
        }
        bool IsReady() {
            if (dbWorker == nullptr) {
                Core::sysLogger->LogError("profile cache", "dbWorker not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;

    public:
        uint16_t GetBatch(const uint32_t* profileIds, uint16_t count, Core::ProfileEntry* out) override;
        void Load(uint32_t profileId, uint32_t version, const std::string& name);
        void Unload(uint32_t profileId);
        void Rename(uint32_t profileId, const std::string& name, std::function<void(bool, uint32_t)> onDone);
    };
}
