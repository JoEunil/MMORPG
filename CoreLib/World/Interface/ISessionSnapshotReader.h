#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <BaseLib/TripleBuffer.h>

namespace Core {
    using SessionSnapshot = std::vector<std::vector<uint64_t>>;
    using SessionSnapshotView = Base::BufferReader<SessionSnapshot>;

    class ISessionSnapshotReader {
    public:
        virtual ~ISessionSnapshotReader() = default;
        virtual std::optional<SessionSnapshotView> ReadSessionSnapshot(uint16_t zoneID) const = 0;
    };
}
