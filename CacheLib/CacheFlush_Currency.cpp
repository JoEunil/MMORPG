#include "CacheFlush.h"

namespace Cache {
    void CacheFlush::FlushCurrency(FlushCommand* command) {
        const CurrencyKey key{ std::any_cast<uint64_t>(command->params[1]) };
        const auto shardIndex = static_cast<uint16_t>(key.characterID & SHARD_SIZE_MASK);
        auto& gold = std::any_cast<uint64_t&>(command->params[0]);
        if (!TryWriteStorage(*command, *cache_currency, key, shardIndex, gold))
            return;

        cache_currency->WriteDone(shardIndex, key, command->generation);
    }
}
