#pragma once

#pragma comment(lib, "ws2_32")
#pragma comment(lib,"mswsock.lib")
// AcceptEx()
#include <winsock2.h>
// 기본 socket 함수, windows.h 보다 먼저 와야됨
#include <mswsock.h>
// AcceptEx
#include <Ws2tcpip.h>
// 추가적인 tcp/ip 기능 getaddrinfo(), inet_pton()

#include <thread>
#include <atomic>
#include <cstdint>
#include <CoreLib/IIOCP.h>
#include <CoreLib/LoggerGlobal.h>

#include "IAbortSocket.h"
#include "Config.h"

namespace Core {
    class IPacket;
}

namespace Net {
    class NetHandler;
    class OverlappedExPool;
    class SessionManager;
    class NetPerfCollector;
    class IOCP : public Core::IIOCP, public IAbortSocket {
        HANDLE m_hIOCP;
        std::vector<std::thread> m_threads;
        SOCKET m_listenSock;
        LPFN_ACCEPTEX m_lpfnAcceptEx = nullptr; // AcceptEx 함수를 담을 함수 포인터
        std::atomic<bool> m_isRunning = false;
        std::atomic<bool> m_receiving = false;

        std::atomic<bool>* fatalError;
        std::condition_variable* cv;

        OverlappedExPool* overlappedExPool;
        NetHandler* netHandler;
        SessionManager* sessionManager;
        NetPerfCollector* perfCollector;
        void Initialize(OverlappedExPool* o, NetHandler* n, SessionManager* s, std::atomic<bool>* f, std::condition_variable* c, NetPerfCollector* p) {
            overlappedExPool = o;
            netHandler = n;
            sessionManager = s;
            fatalError = f;
            cv = c;
            perfCollector = p;
        }
        bool IsReady() {
            if (!m_isRunning.load()) {
                Core::sysLogger->LogError("iocp", "not running");
                return false;
            }
            if (!m_receiving.load()) {
                Core::sysLogger->LogError("iocp", "not receiving");
                return false;
            }
            if (overlappedExPool == nullptr) {
                Core::sysLogger->LogError("iocp", "overlappedExPool not initialized");
                return false;
            }
            if (netHandler == nullptr) {
                Core::sysLogger->LogError("iocp", "netHandler not initialized");
                return false;
            }
            if (fatalError == nullptr) {
                Core::sysLogger->LogError("iocp", "fatalError not initialized");
                return false;
            }
            if (cv == nullptr) {
                Core::sysLogger->LogError("iocp", "cv not initialized");
                return false;
            }
            if (perfCollector == nullptr) {
                Core::sysLogger->LogError("iocp", "perfCollector not initialized");
                return false;
            }
           return true;
        }
        void WorkerThreadFunc(int index);
        bool CreateListenSocket();
        bool CreateWorkerThread();
        
        void PostAccept();
        bool PostRecv(SOCKET clientSocket);
        void CleanUpSocket(SOCKET clientSocket);
        
        void Start();
        void StopReceive() {
            m_receiving.store(false);
        }
        void CleanUp();
        
        friend class Initializer;
    public:
        ~IOCP() {
            CleanUp();
        }
        void SendData(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet) override;
        void SendDataUnique(uint64_t sessionID, std::unique_ptr<Core::IPacket, Core::PacketDeleter> packet) override;

        void AbortSocket(SOCKET sock) override;
    };
}
