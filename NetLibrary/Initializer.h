#pragma once

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <conio.h>
#include <array>

#include "OverlappedExPool.h"
#include "NetHandler.h"
#include "IOCP.h"
#include "ClientContext.h"
#include "ClientContextPool.h"
#include "PacketPool.h"
#include "NetPacketFilter.h"
#include "IAbortSocket.h"
#include "PingManager.h"
#include "SessionManager.h"
#include "NetTimer.h"

#include <CoreLib/ILogger.h>
#include <CoreLib/IIocp.h>
#include <coreLib/IPacketPool.h>
#include <coreLib/IPacketDispatcher.h>

#include "Config.h"
namespace Net {
    class Initializer {
        IOCP iocp;
        OverlappedExPool overlappedExPool;
        NetHandler netHandler;
        std::atomic<bool> fatalError;
        ClientContextPool clientContextPool;
        std::condition_variable cv;
        std::mutex mutex; 
        PacketPool packetPool { TARGET_PACKETPOOL_SIZE, MAX_PACKETPOOL_SIZE, MIN_PACKETPOOL_SIZE };
        PacketPool bigPacketPool{ TARGET_BPACKETPOOL_SIZE, MAX_BPACKETPOOL_SIZE, MIN_BPACKETPOOL_SIZE };
        PingManager pingManager;
        SessionManager sessionManager;

    public:
        void Initialize() {
            NetTimer::StartThread();
            overlappedExPool.Initialize();
            clientContextPool.Initialize();
            packetPool.Initialize();
            bigPacketPool.Initialize();
            sessionManager.Initialize();
            netHandler.Initialize(&clientContextPool, &sessionManager, &iocp);
        }
        void CleanUp1() {
            pingManager.StopPing();
            iocp.StopReceive();
        }
        void CleanUp2() {
            iocp.CleanUp();
            NetTimer::Stop();
        }
        
        void InjectDependencies(Core::ILogger* l, Core::IPacketDispatcher* packetDispatcher) {
            iocp.Initialize(l, &overlappedExPool, &netHandler, &sessionManager, &fatalError, &cv);
            pingManager.Initialize(static_cast<IAbortSocket*>(&iocp), &sessionManager);
            NetPacketFilter::Initialize(packetDispatcher);
            iocp.Start();
            pingManager.PingStart();
        }
        
        Core::IIOCP* GetIOCP() {
            return static_cast<Core::IIOCP*>(&iocp);  
        }

        Core::IPacketPool* GetPacketPool() {
            return static_cast<Core::IPacketPool*>(&packetPool);
        }

        Core::IPacketPool* GetBigPacketPool() {
            return static_cast<Core::IPacketPool*>(&bigPacketPool);
        }

        bool CheckReady(Core::ILogger* logger) {
            logger->LogInfo("Net check ready start");
            if (!overlappedExPool.IsReady()) {
                logger->LogError("check ready failed, overlappedExPool");
                return false;
            }
            if (!clientContextPool.IsReady()) {
                logger->LogError("check ready failed, clientContextPool");
                return false;
            }
            if (!packetPool.IsReady()) {
                logger->LogError("check ready failed, packetPool");
                return false;
            }
            if (!iocp.IsReady()) {
                logger->LogError("check ready failed, iocp");
                return false;
            }
            if (!netHandler.IsReady()) {
                logger->LogError("check ready failed, netHandler");
                return false;
            }
            logger->LogWarn("Net check ready success");
            return true;
        }

        void WaitCloseSignal(Core::ILogger* logger) {
            std::unique_lock<std::mutex> lock(mutex);
            while (true) {
                if (fatalError.load()) {
                    logger->LogError("Fatal error 발생, 서버 종료\n");
                    break;
                }

                // 실제 서버에서는 종료 신호 받아서 종료 처리
                if (_kbhit()) {
                    char ch = _getch();
                    if (ch == 'q' || ch == 'Q') {
                        logger->LogWarn("종료 입력 감지\n");
                        std::cout << "서버 종료 신호 수신" << std::endl;
                        break;
                    }
                }
                cv.wait_for(lock, std::chrono::milliseconds(100));
            }
            CleanUp1();
        }
    };
}
