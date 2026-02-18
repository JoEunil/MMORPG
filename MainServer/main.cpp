#include "main.h"

#include <memory>
#include <iostream>
#include <format>

#include <Netlibrary/Initializer.h>
#include <CoreLib/Initializer.h>
#include <CacheLib/Initializer.h>

#include <CoreLib/LoggerGlobal.h>

#include <ExternalLib/Logger.h>
#include <ExternalLib/SessionAuth.h>
#include <External/spdlog/spdlog.h>

#include <mysqlconn/include/mysql/jdbc.h>

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
    try {
        External::SessionAuth auth;
        auth.Initialize();

        Core::Initializer core;
        Net::Initializer net;
        Cache::Initializer cache;

        cache.Initialize();
        core.Initialize();
        net.Initialize();

        core.InjectDependencies1(net.GetIOCP(), net.GetPacketPool(), net.GetBigPacketPool());
        cache.InjectDependencies(core.GetMessageQueue());
        core.InjectDependencies2(net.GetIOCP(), &auth, cache.GetMessageQueue(), net.GetPacketPool());
        net.InjectDependencies(core.GetPacketDispatcher());

        if (net.CheckReady() && core.CheckReady() && cache.CheckReady()) {
            std::cout << "서버 준비 완료" << std::endl;
            net.WaitCloseSignal();
        }
        std::cout << "서버 종료" << std::endl;
        Core::sysLogger->LogInfo("server stop");
        net.CleanUp1();
        core.CleanUp1();
        cache.CleanUp1();
        core.CleanUp2();
        cache.CleanUp2();
        net.CleanUp2();
    }
    catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        Core::errorLogger->LogError(std::format("exception: {}", e.what()));
        return 0;
    }
    catch (...) {
        std::cerr << "Unknown exception caught in main()" << std::endl;
        Core::errorLogger->LogError(std::format("undefined excetion"));
        return 0;
    }
    return 0;
}
