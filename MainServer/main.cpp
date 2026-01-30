#include "main.h"

#include <memory>
#include <iostream>
#include <format>

#include <Netlibrary/Initializer.h>
#include <CoreLib/Initializer.h>
#include <CacheLib/Initializer.h>

#include <CoreLib/ILogger.h>

#include <ExternalLib/Logger.h>
#include <ExternalLib/SessionAuth.h>
#include <External/spdlog/spdlog.h>

#include <mysqlconn/include/mysql/jdbc.h>
#include "UnitTest.h"


int main(int argc, char* argv[]) {
    ST_WSA_INITIALIZER wsa; // winsock 초기화

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::cout << " 서버 시작" << std::endl;
    External::Logger::Initialize();
    External::Logger iocpLogger, netLogger, coreLogger, cacheLogger, authLogger, serverLogger;
    iocpLogger.CreateSink("iocp");
    coreLogger.CreateSink("core");
    cacheLogger.CreateSink("cache");
    authLogger.CreateSink("auth");
    serverLogger.CreateSink("server_lifecycle");

    serverLogger.LogInfo("Server start");
    try {

        External::SessionAuth auth;
        auth.Initialize(&authLogger);

        Core::Initializer core;
        Net::Initializer net;
        Cache::Initializer cache;

        cache.Initialize();
        core.Initialize();
        net.Initialize();

        core.InjectDependencies1(net.GetIOCP(), &coreLogger, net.GetPacketPool(), net.GetBigPacketPool());
        cache.InjectDependencies(core.GetMessageQueue(), &cacheLogger);
        core.InjectDependencies2(net.GetIOCP(), &coreLogger, &auth, cache.GetMessageQueue(), net.GetPacketPool());
        net.InjectDependencies(&iocpLogger, core.GetPacketDispatcher());

        if (net.CheckReady(&serverLogger) && core.CheckReady(&serverLogger) && cache.CheckReady(&serverLogger)) {
            std::cout << "서버 준비 완료" << std::endl;
            net.WaitCloseSignal(&serverLogger);
        }
        std::cout << "서버 종료" << std::endl;
        serverLogger.LogInfo("server stop");
        net.CleanUp1();
        core.CleanUp1();
        cache.CleanUp1();
        core.CleanUp2();
        cache.CleanUp2();
        net.CleanUp2();
    }
    catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        serverLogger.LogError(std::format("exception: {}", e.what()));
        return 0;
    }
    catch (...) {
        std::cerr << "Unknown exception caught in main()" << std::endl;
        serverLogger.LogError(std::format("undefined excetion"));
        return 0;
    }
    return 0;
}
