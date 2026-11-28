#include <BaseLib/WAL.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <thread>

namespace {
    using Clock = std::chrono::steady_clock;
    enum class Mode { Write, Sync };
    const char* Name(Mode mode) {
        switch (mode) {
        case Mode::Write: return "write_fflush";
        case Mode::Sync: return "write_fsync";
        }
        throw std::runtime_error("Unknown mode");
    }

    struct Options {
        uint64_t samples = 5000;
        uint64_t warmup = 20;
        uint64_t rounds = 3;
        uint64_t caseSeconds = 2;
        std::vector<size_t> sizes{64, 512, 4096};
        std::filesystem::path directory = "benchmark-output/wal-latency";
        bool raw = false;
    };

    uint64_t PositiveNumber(const std::string& text, bool allowZero = false) {
        if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
            throw std::runtime_error("Expected an unsigned integer: " + text);
        size_t consumed = 0;
        const auto value = std::stoull(text, &consumed);
        if (consumed != text.size() || (!allowZero && value == 0))
            throw std::runtime_error("Expected a positive integer: " + text);
        return value;
    }

    Options Parse(int argc, char** argv) {
        Options options;
        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--raw") { options.raw = true; continue; }
            if (arg != "--samples" && arg != "--warmup" && arg != "--rounds" && arg != "--sizes" && arg != "--directory" && arg != "--case-seconds")
                throw std::runtime_error("Unknown argument: " + arg);
            if (++index == argc) throw std::runtime_error("Missing value for " + arg);
            const std::string value = argv[index];
            if (arg == "--samples") options.samples = PositiveNumber(value);
            else if (arg == "--warmup") options.warmup = PositiveNumber(value, true);
            else if (arg == "--rounds") options.rounds = PositiveNumber(value);
            else if (arg == "--directory") options.directory = value;
            else if (arg == "--case-seconds") options.caseSeconds = PositiveNumber(value, true);
            else {
                options.sizes.clear();
                std::istringstream sizes(value);
                std::string size;
                while (std::getline(sizes, size, ',')) options.sizes.push_back(static_cast<size_t>(PositiveNumber(size)));
                if (options.sizes.size() != 3 || value.back() == ',')
                    throw std::runtime_error("--sizes requires three distinct payload sizes, e.g. 64,512,4096");
            }
        }
        auto sizes = options.sizes;
        std::sort(sizes.begin(), sizes.end());
        if (std::adjacent_find(sizes.begin(), sizes.end()) != sizes.end())
            throw std::runtime_error("Payload sizes must be distinct");
        for (const auto size : sizes) {
            if (size > UINT16_MAX - sizeof(Base::WALHeader)) throw std::runtime_error("Payload exceeds WAL record limit");
            const auto capacity = UINT32_MAX / (size + sizeof(Base::WALHeader));
            if (options.samples > capacity || options.warmup > capacity - options.samples)
                throw std::runtime_error("Samples plus warmup would rotate the segment; reduce count or payload size");
        }
        if (options.rounds > UINT32_MAX / 6) throw std::runtime_error("Too many rounds");
        return options;
    }

    double Microseconds(Clock::duration duration) {
        return std::chrono::duration<double, std::micro>(duration).count();
    }

    double Percentile(const std::vector<double>& sorted, double quantile) {
        // Nearest rank over individual calls, not over run averages.
        return sorted[static_cast<size_t>(std::ceil(quantile * sorted.size())) - 1];
    }

    struct Scenario { Mode mode; size_t bytes; };
    struct Result {
        uint64_t round;
        size_t order;
        Scenario scenario;
        uint64_t samples;
        double mean, p50, p95, p99, minimum, maximum, total, measuredWall, finalSync;
        std::vector<double> raw;
    };

    void WriteOnce(Base::WAL& wal, std::vector<uint8_t>& payload, Mode mode) {
        if (wal.Write(payload.data(), payload.size(), 1) == 0) throw std::runtime_error("WAL Write failed");
        if (mode == Mode::Sync && !wal.Fsync()) throw std::runtime_error("Fsync failed");
    }

    Result Measure(const Options& options, const Scenario& scenario, uint64_t round, size_t order,
        const std::filesystem::path& directory) {
        const auto file = directory / ("r" + std::to_string(round) + "_" + Name(scenario.mode) + "_" + std::to_string(scenario.bytes));
        std::vector<uint8_t> payload(scenario.bytes, 0x5a);
        std::vector<double> latency;
        latency.reserve(static_cast<size_t>(options.samples));
        double finalSync = 0, measuredWall = 0;
        {
            Base::WAL wal(file.string(), UINT32_MAX, [](const Base::WALHeader&, const uint8_t*) {});
            for (uint64_t index = 0; index < options.warmup; ++index) WriteOnce(wal, payload, scenario.mode);
            if (!wal.Fsync()) throw std::runtime_error("Warmup drain failed");
            const auto measurementBegin = Clock::now();
            for (uint64_t index = 0; index < options.samples; ++index) {
                std::memcpy(payload.data(), &index, (std::min)(sizeof(index), payload.size()));
                const auto begin = Clock::now();
                WriteOnce(wal, payload, scenario.mode);
                const auto end = Clock::now();
                latency.push_back(Microseconds(end - begin));
                if (options.caseSeconds != 0 && Microseconds(end - measurementBegin) >= options.caseSeconds * 1e6)
                    break;
            }
            measuredWall = Microseconds(Clock::now() - measurementBegin);
            const auto begin = Clock::now();
            if (!wal.Fsync()) throw std::runtime_error("Final drain failed");
            finalSync = Microseconds(Clock::now() - begin);
        }
        // One fresh segment per scenario; delete only the exact file we created.
        if (!std::filesystem::remove(file.string() + ".1")) throw std::runtime_error("Cannot remove scenario WAL file");
        double total = 0;
        for (const auto value : latency) total += value;
        auto sorted = latency;
        std::sort(sorted.begin(), sorted.end());
        Result result{round, order, scenario, static_cast<uint64_t>(latency.size()), total / latency.size(), Percentile(sorted, .50), Percentile(sorted, .95),
            Percentile(sorted, .99), sorted.front(), sorted.back(), total, measuredWall, finalSync, {}};
        if (options.raw) result.raw = std::move(latency);
        return result;
    }

    double TimerOverhead() {
        std::vector<double> times;
        for (int index = 0; index < 10000; ++index) {
            const auto begin = Clock::now();
            const auto end = Clock::now();
            times.push_back(Microseconds(end - begin));
        }
        std::sort(times.begin(), times.end());
        return Percentile(times, .50);
    }

    void Export(const Options& options, const std::filesystem::path& directory, const std::vector<Result>& results) {
        std::ofstream summary(directory / "summary.csv");
        summary.exceptions(std::ios::badbit | std::ios::failbit);
        summary << "round,order,mode,payload_bytes,record_bytes,samples,mean_us,p50_us,p95_us,p99_us,min_us,max_us,total_call_us,measured_wall_us,final_sync_us,throughput_ops_s,throughput_with_final_sync_ops_s\n";
        summary << std::fixed << std::setprecision(6);
        for (const auto& result : results) {
            summary << result.round << ',' << result.order << ',' << Name(result.scenario.mode) << ',' << result.scenario.bytes << ','
                << result.scenario.bytes + sizeof(Base::WALHeader) << ',' << result.samples << ',' << result.mean << ',' << result.p50 << ','
                << result.p95 << ',' << result.p99 << ',' << result.minimum << ',' << result.maximum << ',' << result.total << ',' << result.measuredWall << ',' << result.finalSync << ','
                << result.samples * 1e6 / result.measuredWall << ',' << result.samples * 1e6 / (result.measuredWall + result.finalSync) << '\n';
        }
        summary.close();
        if (options.raw) {
            std::ofstream samples(directory / "samples.csv");
            samples.exceptions(std::ios::badbit | std::ios::failbit);
            samples << "round,order,mode,payload_bytes,sample,latency_us\n" << std::fixed << std::setprecision(6);
            for (const auto& result : results)
                for (size_t index = 0; index < result.raw.size(); ++index)
                    samples << result.round << ',' << result.order << ',' << Name(result.scenario.mode) << ',' << result.scenario.bytes << ','
                        << index + 1 << ',' << result.raw[index] << '\n';
            samples.close();
        }
    }
}

int RunWALWriteLatency(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Benchmark --wal-latency [--samples 5000] [--rounds 3] [--warmup 20] [--case-seconds 2] [--sizes 64,512,4096] [--directory PATH] [--raw]\n";
            return 0;
        }
        const auto options = Parse(argc, argv);
        const auto directory = std::filesystem::absolute(options.directory) /
            (std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "_" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(directory.parent_path());
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("Result directory already exists");
        constexpr uint32_t seed = 20261128;
        std::mt19937 random(seed);
        const auto timerUs = TimerOverhead();
        std::ofstream metadata(directory / "metadata.txt");
        metadata.exceptions(std::ios::badbit | std::ios::failbit);
        metadata << "samples_per_case=" << options.samples << "\nrounds=" << options.rounds << "\nwarmup=" << options.warmup
            << "\ncase_seconds=" << options.caseSeconds << "\nseed=" << seed << "\ntimer_median_us=" << timerUs << "\nlogical_processors=" << std::thread::hardware_concurrency()
            << "\ncompiler=" << _MSC_FULL_VER << "\nwrite_policy=fflush_on_write\nwriter_threads=1\nbackground_fsync=none\npercentiles=nearest_rank_per_call\n";
#ifdef _DEBUG
        metadata << "build=Debug\n";
        std::cerr << "Warning: use Release x64 for performance comparisons.\n";
#else
        metadata << "build=Release\n";
#endif
        metadata.close();
        std::vector<Scenario> scenarios;
        for (const auto bytes : options.sizes)
            for (auto mode : {Mode::Write, Mode::Sync}) scenarios.push_back({mode, bytes});
        std::vector<Result> results;
        std::cout << "Output: " << directory << "\nTimer median: " << timerUs << " us (not subtracted)\n";
        std::cout << "round mode payload samples mean_us p50_us p95_us p99_us ops_s ops_s_with_final_sync\n" << std::fixed << std::setprecision(3);
        for (uint64_t round = 1; round <= options.rounds; ++round) {
            std::shuffle(scenarios.begin(), scenarios.end(), random);
            for (size_t order = 0; order < scenarios.size(); ++order) {
                const auto& scenario = scenarios[order];
                std::cout << "Running round " << round << ": " << Name(scenario.mode) << ' ' << scenario.bytes << "B...\n" << std::flush;
                auto result = Measure(options, scenario, round, order + 1, directory);
                std::cout << round << ' ' << Name(scenario.mode) << ' ' << scenario.bytes << ' ' << result.samples << ' ' << result.mean << ' ' << result.p50 << ' '
                    << result.p95 << ' ' << result.p99 << ' ' << result.samples * 1e6 / result.measuredWall << ' '
                    << result.samples * 1e6 / (result.measuredWall + result.finalSync) << '\n';
                results.push_back(std::move(result));
            }
        }
        // Export after all measurements so CSV writes do not compete with measured I/O.
        Export(options, directory, results);
        std::cout << "Saved " << results.size() << " cases to summary.csv\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed (run incomplete): " << error.what() << '\n';
        return 1;
    }
}
