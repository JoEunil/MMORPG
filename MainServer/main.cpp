#include "main.h"

#include <memory>
#include <iostream>
#include <format>
#include <cstdlib>

#include <Netlibrary/Initializer.h>
#include <CoreLib/Initializer.h>
#include <CoreLib/DurableLogger.h>
#include <CacheLib/Initializer.h>

#include <CoreLib/LoggerGlobal.h>

#include <ExternalLib/Logger.h>
#include <ExternalLib/SessionAuth.h>
#include <External/spdlog/spdlog.h>

#include <mysqlconn/include/mysql/jdbc.h>

#include "IntegrationTestBazaar.h"
#include "CacheDurabilityTest.h"
#include "IntegrationTestCache.h"

int main(int argc, char* argv[]) {
    ST_WSA_INITIALIZER wsa; // winsock 초기화

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::cout << " 서버 시작" << std::endl;
    External::Logger::Initialize();

    Core::sysLogger = std::make_unique<External::Logger>();
    Core::sysLogger->CreateSink("system");
    Core::gameLogger = std::make_unique<External::Logger>();
    Core::gameLogger->CreateSink("game");
    Core::errorLogger = std::make_unique<External::Logger>();
    Core::errorLogger->CreateSink("error");
    Core::perfLogger = std::make_unique<External::Logger>();
    Core::perfLogger->CreateSink("perf");
    bool testBazaar = !Cache::GetEnvVar("TEST_BAZAAR").empty();
    bool testCache = !Cache::GetEnvVar("TEST_CACHE").empty();
    bool testDurable = !Cache::GetEnvVar("TEST_CACHE_DURABLE").empty();
    if (testBazaar || testCache || testDurable) {
        int result = EXIT_FAILURE;
        try {
            if (testBazaar) {
                Test::g_msgPool().InitializeForTest();
                Test::RunAll();
                result = EXIT_SUCCESS;
            }
            else if (testCache) {
                result = Cache::RunAllCacheTests();
            }
            else {
                result = Cache::RunCacheDurabilityTest();
            }
        }
        catch (const sql::SQLException& e) {
            std::cerr << std::format("SQLException: {} (code: {}, state: {})\n", e.what(), e.getErrorCode(), e.getSQLState());
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown integration test exception\n";
        }
        Core::sysLogger.reset();
        Core::gameLogger.reset();
        Core::errorLogger.reset();
        Core::perfLogger.reset();
        spdlog::shutdown();
        return result;
    }
    try {
        External::SessionAuth auth;
        auth.Initialize();

        Core::DurableLogger durableLogger;
        durableLogger.Initialize();
        Core::Initializer core;
        Net::Initializer net;
        Cache::Initializer cache;
        // Preserve the same shutdown order on normal exit and exception unwinding.
        struct Shutdown {
            External::SessionAuth& auth;
            Core::Initializer& core;
            Net::Initializer& net;
            Cache::Initializer& cache;
            bool stopped = false;
            void Run() {
                if (stopped) return;
                net.CleanUp1();
                auth.Stop();
                core.CleanUp1();
                core.CleanUp2();
                cache.CleanUp();
                net.CleanUp2();
                stopped = true;
            }
            ~Shutdown() { Run(); }
        } shutdown{auth, core, net, cache};

        cache.Initialize();
        core.Initialize();
        net.Initialize();

        core.InjectDependencies1(net.GetIOCP(), net.GetPacketPool(), net.GetBigPacketPool(), net.GetGiantPacketPool(), &durableLogger);
        cache.InjectDependencies(core.GetMessageQueue(), &durableLogger);
        core.InjectDependencies2(net.GetIOCP(), &auth, cache.GetMessageQueue(), net.GetPacketPool(), cache.GetProfileCache(), cache.GetInventoryOperations());
        net.InjectDependencies(core.GetPacketDispatcher());

        if (net.CheckReady() && core.CheckReady() && cache.CheckReady()) {
            std::cout << "서버 준비 완료" << std::endl;
            net.WaitCloseSignal();
        }
        else {
            if (!net.CheckReady())
                std::cout << "net 체크 실패" << std::endl;
            if (!core.CheckReady())
            std::cout << "core 체크 실패" << std::endl;
            if (!cache.CheckReady())
            std::cout << "cache 체크 실패" << std::endl;
        }
        std::cout << "서버 종료" << std::endl;
        Core::sysLogger->LogInfo("server stop");
        // === Graceful shutdown ===
        // 자원 소유/의존 순서에 맞춰 정리한다. 특히 종료 시점의 캐릭터 상태는
        // stateManager.CleanUp()(core.CleanUp2) 에서 cache 큐로 enqueue → cache.CleanUp()의
        // recvMQ drain → dbWorker drain 경로로 유실 없이 DB에 기록된다.
        // 각 Stop()/CleanUp()은 idempotent(running.exchange) + 로거 null-safe 이며,
        // DB 워커는 큐를 drain한 뒤 종료한다. 상세: GracefulShutdown.md
        shutdown.Run();

        Core::sysLogger.reset();
        Core::gameLogger.reset();
        Core::errorLogger.reset();
        Core::perfLogger.reset();
        spdlog::shutdown();
    }
    catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        Core::errorLogger->LogError(std::format("exception: {}", e.what()));
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "Unknown exception caught in main()" << std::endl;
        Core::errorLogger->LogError(std::format("undefined excetion"));
        return EXIT_FAILURE;
    }
    return 0;
}

