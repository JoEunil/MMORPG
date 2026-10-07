#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    double Percentile(const std::vector<double>& values, double ratio) {
        if (values.empty())
            return 0.0;

        auto sorted = values;
        std::sort(sorted.begin(), sorted.end());

        const double position =
            ratio * static_cast<double>(sorted.size() - 1);

        const size_t lower =
            static_cast<size_t>(std::floor(position));
        const size_t upper =
            static_cast<size_t>(std::ceil(position));
        const double weight =
            position - static_cast<double>(lower);

        return sorted[lower] * (1.0 - weight)
            + sorted[upper] * weight;
    }

    double P01(const std::vector<double>& values) {
        return Percentile(values, 0.01);
    }

    double P05(const std::vector<double>& values) {
        return Percentile(values, 0.05);
    }

    double P50(const std::vector<double>& values) {
        return Percentile(values, 0.50);
    }

    double P95(const std::vector<double>& values) {
        return Percentile(values, 0.95);
    }

    double P99(const std::vector<double>& values) {
        return Percentile(values, 0.99);
    }

}