#include "pch.h"
#include "IOCP.h"

#include <CoreLib/LoggerGlobal.h>
#include "OverlappedExPool.h"

namespace Net {
    void IOCP::PostAccept()
    {
        if (!m_isRunning.load(std::memory_order_relaxed))
            return;
        SOCKET clientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (clientSocket == INVALID_SOCKET) {
            Core::errorLogger->LogWarn("iocp", "failed to create client socket");
            return;
        }

        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        if (!pOverlappedEx) {
            Core::errorLogger->LogWarn("iocp", "failed to acquire overlappedEx");
            closesocket(clientSocket);
            return;
        }
        // wsaOverlapped만 초기화
        ZeroMemory(&pOverlappedEx->wsaOverlapped, sizeof(WSAOVERLAPPED));
        pOverlappedEx->op = IOOperation::ACCEPT;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.resize(1);
        pOverlappedEx->wsaBuf[0].buf = overlappedExPool->AcquireAcceptBuffer();
        if (pOverlappedEx->wsaBuf[0].buf == nullptr) {
            // PREPOSTED_ACCEPT 만큼만 PostAccept가 유지되기 때문에 발생할 일이 없음. Accept가 실패될 순 있어도 PostAccept가 실패될 순 없음
            Core::errorLogger->LogError("iocp", "Failed to acquire Accept Buffer");
            overlappedExPool->Return(pOverlappedEx);
            closesocket(clientSocket);
            return;
        }
        if (!m_isRunning.load(std::memory_order_relaxed)) {
            overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf[0].buf);
            overlappedExPool->Return(pOverlappedEx);
            closesocket(clientSocket);
            return;
        }
        DWORD bytesReceived = 0;
        BOOL bRet = m_lpfnAcceptEx(
            m_listenSock,
            clientSocket,
            pOverlappedEx->wsaBuf[0].buf,
            0,
            sizeof(SOCKADDR_IN) + 16,
            sizeof(SOCKADDR_IN) + 16,
            &bytesReceived,
            (LPOVERLAPPED)pOverlappedEx
        );

        if (bRet == FALSE) {
            int err = WSAGetLastError();
            if (err != ERROR_IO_PENDING) {
                const bool expectedShutdown = !m_isRunning.load(std::memory_order_relaxed) &&
                    !m_receiving.load(std::memory_order_relaxed) &&
                    (err == ERROR_OPERATION_ABORTED || err == WSAENOTSOCK);
                if (!expectedShutdown) {
                    Core::errorLogger->LogError("iocp", "Critical: failed to post AcceptEx", "error", err);

                    m_isRunning.store(false, std::memory_order_relaxed);
                    fatalError->store(true, std::memory_order_relaxed);
                    cv->notify_one();
                }

                overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf[0].buf);
                overlappedExPool->Return(pOverlappedEx);
                closesocket(clientSocket);
            }
        }
    }
}