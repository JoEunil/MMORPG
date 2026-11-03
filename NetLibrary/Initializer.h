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
#include "PacketPoolManager.h"
#include "NetPacketFilter.h"
#include "IAbortSocket.h"
#include "PingManager.h"
#include "SessionManager.h"
#include "NetTimer.h"
#include "NetPerfCollector.h"

#include <CoreLib/ILogger.h>
#include <CoreLib/IIocp.h>
#include <CoreLib/IPacketPoolManager.h>
#include <coreLib/IPacketDispatcher.h>

#include "Config.h"
namespace Net {
    class Initializer {
        IOCP iocp;
        // 소멸 순서 주의: STOverlappedEx가 packetPoolManager 소유 풀의 패킷을 물고 있고,
        // ClientContext는 overlappedExPool의 overlapped를 물고 있음.
        // 선언 역순으로 소멸하므로 packetPoolManager → overlappedExPool → clientContextPool 순으로 선언해
        // 소멸 시 clientContextPool → overlappedExPool → packetPoolManager 순으로 파괴되게 함(자원 제공자가 더 늦게 파괴).
        PacketPoolManager packetPoolManager;
        OverlappedExPool overlappedExPool;
        NetHandler netHandler;
        std::atomic<bool> fatalError;
        ClientContextPool clientContextPool;
        std::condition_variable cv;
        std::mutex mutex;
        PingManager pingManager;
        SessionManager sessionManager;
        NetPerfCollector perfCollector;
    public:
        ~Initializer() {
            StopProducers();
            FinishShutdown();
		}
        void Initialize() {
            NetTimer::StartThread();
            clientContextPool.Initialize();
            ClientContext::Initialize(&overlappedExPool, &sessionManager);
            sessionManager.Initialize(&clientContextPool);
            netHandler.Initialize(&sessionManager, &iocp);
            perfCollector.Initialize(&sessionManager, &packetPoolManager, &overlappedExPool,&clientContextPool);
        }
        void StopProducers() {
            pingManager.StopPing();
            iocp.StopReceive();
            perfCollector.Stop();
        }
        void FinishShutdown() {
            iocp.CleanUp();
            NetTimer::Stop();
        }
        
        void InjectDependencies(Core::IPacketDispatcher* packetDispatcher) {
            iocp.Initialize(&overlappedExPool, &netHandler, &sessionManager, &fatalError, &cv, &perfCollector);
            pingManager.Initialize(static_cast<IAbortSocket*>(&iocp), &sessionManager, packetDispatcher);
            NetPacketFilter::Initialize(packetDispatcher, &perfCollector);
            iocp.Start();
            pingManager.PingStart();
            perfCollector.Start();
        }
        
        Core::IIOCP* GetIOCP() {
            return static_cast<Core::IIOCP*>(&iocp);  
        }

        Core::IPacketPoolManager* GetPacketPoolManager() {
            return &packetPoolManager;
        }

        bool CheckReady() const {
            if (!overlappedExPool.IsReady()
                or !clientContextPool.IsReady()
                or packetPoolManager.IsReady()
                or !iocp.IsReady()
                or !netHandler.IsReady()
                or !pingManager.IsReady() 
                or !perfCollector.IsReady() 
                or !ClientContext::IsReady()) {
                return false;
            }
            return true;
        }

        void WaitCloseSignal() {
            std::unique_lock<std::mutex> lock(mutex);
            while (true) {
                if (fatalError.load(std::memory_order_relaxed)) {
                    Core::errorLogger->LogError("signal", "Fatal error Occured");
                    break;
                }

                // 실제 서버에서는 종료 신호 받아서 종료 처리
                if (_kbhit()) {
                    char ch = _getch();
                    if (ch == 'q' || ch == 'Q') {
                        Core::sysLogger->LogWarn("signal", "Close signal received");
                        break;
                    }
                }
                cv.wait_for(lock, std::chrono::milliseconds(100));
            }
        }
    };
}
