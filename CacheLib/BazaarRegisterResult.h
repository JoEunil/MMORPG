#pragma once
#include <cstdint>

namespace Cache {
    // Only a returned zero proves that INSERT failed. An exception may follow COMMIT.
    // Restore is attempted once: a partial restore must never be retried automatically.
    template<typename Insert, typename Restore, typename ReportUnknown>
    uint8_t ResolveBazaarRegister(Insert&& insert, Restore&& restore, ReportUnknown&& reportUnknown) {
        int affected;
        try { affected = insert(); }
        catch (...) {
            reportUnknown();
            return 5;
        }
        if (affected == 1) return 1;
        if (affected == 0) {
            try { if (restore()) return 0; }
            catch (...) {} // A restore may have applied only one resource; do not repeat it.
        }
        reportUnknown();
        return 5;
    }
}
