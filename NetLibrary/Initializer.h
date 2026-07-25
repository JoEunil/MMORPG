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
#include "NetPerfCollector.h"

#include <CoreLib/ILogger.h>
#include <CoreLib/IIocp.h>
#include <coreLib/IPacketPool.h>
#include <coreLib/IPacketDispatcher.h>

#include "Config.h"
namespace Net {
    class Initializer {
        IOCP iocp;
        // 소멸 순서 주의: STOverlappedEx가 packetPool/bigPacketPool의 패킷을 물고 있고,
        // ClientContext는 overlappedExPool의 overlapped를 물고 있음.
        // 선언 역순으로 소멸하므로 packetPool/bigPacketPool → overlappedExPool → clientContextPool 순으로 선언해
        // 소멸 시 clientContextPool → overlappedExPool → packetPool 순으로 파괴되게 함(자원 제공자가 더 늦게 파괴).
        PacketPool<PACKETPOOL_SIZE> packetPool{ NORMAL_PACKET_LENGTH };
        PacketPool<BPACKETPOOL_SIZE> bigPacketPool{ BIG_PACKET_LENGTH };
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
        void Initialize() {
            NetTimer::StartThread();
            overlappedExPool.Initialize();
            clientContextPool.Initialize();
            ClientContext::Initialize(&overlappedExPool);
            sessionManager.Initialize(&clientContextPool);
            netHandler.Initialize(&sessionManager, &iocp);
            perfCollector.Initialize(&sessionManager, &packetPool, &bigPacketPool, &overlappedExPool,&clientContextPool);
        }
        void CleanUp1() {
            pingManager.StopPing();
            iocp.StopReceive();
            perfCollector.Stop();
        }
        void CleanUp2() {
            iocp.CleanUp();
            NetTimer::Stop();
        }
        
        void InjectDependencies(Core::IPacketDispatcher* packetDispatcher) {
            iocp.Initialize(&overlappedExPool, &netHandler, &sessionManager, &fatalError, &cv, &perfCollector);
            pingManager.Initialize(static_cast<IAbortSocket*>(&iocp), &sessionManager);
            NetPacketFilter::Initialize(packetDispatcher, &sessionManager, &perfCollector);
            iocp.Start();
            pingManager.PingStart();
            perfCollector.Start();
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

        bool CheckReady() {
            if (!overlappedExPool.IsReady()) {
                return false;
            }
            if (!clientContextPool.IsReady()) {
                return false;
            }
            if (!packetPool.IsReady()) {
                return false;
            }
            if (!iocp.IsReady()) {
                return false;
            }
            if (!netHandler.IsReady()) {
                return false;
            }
            if (!pingManager.IsReady()) {
                return false;
            }
            if (!perfCollector.IsReady()) {
                return false;
            }
            if (!ClientContext::IsReady()) {
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
            CleanUp1();
        }
    };
}
