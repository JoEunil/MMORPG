#include "ProfileCache.h"

#include <CoreLib/LoggerGlobal.h>

namespace Cache {

    uint16_t ProfileCache::GetBatch(const uint32_t* profileIds, uint16_t count, Core::ProfileEntry* out) {
        std::shared_lock lock(m_mutex);
        uint16_t found = 0;
        for (uint16_t i = 0; i < count; i++) {
            auto it = m_profiles.find(profileIds[i]);
            if (it == m_profiles.end())
                continue;

            auto& entry = out[found++];
            entry.profileId = profileIds[i];
            entry.version = it->second.version;
            std::memset(entry.name, 0, sizeof(entry.name));
            std::memcpy(entry.name, it->second.name.c_str(), std::min(it->second.name.size(), sizeof(entry.name) - 1));
        }
        return found;
    }

    void ProfileCache::Load(uint32_t profileId, uint32_t version, const std::string& name) {
        std::unique_lock lock(m_mutex);
        m_profiles[profileId] = ProfileData{ version, name };
    }

    void ProfileCache::Unload(uint32_t profileId) {
        std::unique_lock lock(m_mutex);
        m_profiles.erase(profileId);
    }

    void ProfileCache::Rename(uint32_t profileId, const std::string& name, std::function<void(bool, uint32_t)> onDone) {
        uint32_t nextVersion;
        {
            std::unique_lock lock(m_mutex);
            auto it = m_profiles.find(profileId);
            if (it == m_profiles.end() || it->second.renaming) {
                lock.unlock();
                onDone(false, 0);
                return;
            }
            it->second.renaming = true;
            nextVersion = it->second.version + 1;
        }

        // in-flight를 1개로 제한했으므로 UPDATE가 성공하면 nextVersion이 곧 DB 확정값이다.
        // 재조회 없이 그대로 캐시에 반영한다.
        dbWorker->Enqueue([this, profileId, name, nextVersion, onDone](DBConnectionGame* conn) {
            bool success = conn->ExecuteUpdate(22, name, profileId) > 0;

            std::unique_lock lock(m_mutex);
            auto it = m_profiles.find(profileId);
            if (it != m_profiles.end()) {
                it->second.renaming = false;
                if (success) {
                    it->second.version = nextVersion;
                    it->second.name = name;
                }
            }
            lock.unlock();

            onDone(success, success ? nextVersion : 0);
            });
    }
}