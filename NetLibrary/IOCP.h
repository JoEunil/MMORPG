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
#include <CoreLib/Ilogger.h>

#include "IAbortSocket.h"
#include "Config.h"

namespace Core {
    class IPacket;
    class ILogger;
}

namespace Net {
    class NetHandler;
    class OverlappedExPool;
    class SessionManager;

    class IOCP : public Core::IIOCP, public IAbortSocket {
        HANDLE m_hIOCP;
        std::vector<std::thread> m_threads;
        SOCKET m_listenSock;
        LPFN_ACCEPTEX m_lpfnAcceptEx = nullptr; // AcceptEx 함수를 담을 함수 포인터
        std::atomic<bool> m_isRunning = false;
        std::atomic<bool> m_receiving = false;

        std::atomic<bool>* fatalError;
        std::condition_variable* cv;
        Core::ILogger* logger;

        OverlappedExPool* overlappedExPool;
        NetHandler* netHandler;
        SessionManager* sessionManager;
        
        void Initialize(Core::ILogger* l, OverlappedExPool* o, NetHandler* n, SessionManager* s, std::atomic<bool>* f, std::condition_variable* c) {
            logger = l;
            overlappedExPool = o;
            netHandler = n;
            sessionManager = s;
            fatalError = f;
            cv = c;
        }
        bool IsReady() {
            if (!m_isRunning.load()) {
                logger->LogError("m_isRunning");
                return false;
            }
            if (overlappedExPool == nullptr) {
                logger->LogError("overlappedExPool");
                return false;
            }
            if (netHandler == nullptr) {
                logger->LogError("netHandler");
                return false;
            }
            if (fatalError == nullptr) {
                logger->LogError("fatalError");
                return false;
            }
            if (cv == nullptr) {
                logger->LogError("cv");
                return false;
            }
           return true;
        }
        void WorkerThreadFunc();
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
