#include "pch.h"
#include "IOCP.h"
#include <CoreLib/LoggerGlobal.h>
#include "OverlappedExPool.h"
#include "SessionManager.h"
#include "NetHandler.h"
#include "NetPerfCollector.h"

namespace Net {
    void IOCP::HandleRecvCompletion(STOverlappedEx* work, DWORD bytesTransferred, int index) {
        if (!m_receiving.load(std::memory_order_relaxed))
            return;
        const SOCKET clientSocket = work->clientSocket;
        if (bytesTransferred == 0) {
            Core::sysLogger->LogInfo("iocp", "Client FIN received", "socket", clientSocket);
            CleanUpSocket(clientSocket, work->sessionID);
            return;
        }
        if (work->ownerContext != nullptr && work->ownerContext->IsSessionAlive(work->sessionID)) {
            perfCollector->AddRecvCnt(index);
            netHandler->OnRecv(work->ownerContext, work->sessionID, clientSocket,
                reinterpret_cast<uint8_t*>(work->wsaBuf[0].buf), bytesTransferred);

            if (work->ownerContext->IsSessionAlive(work->sessionID) && !PostRecv(clientSocket, work->sessionID)) {
                Core::errorLogger->LogWarn("iocp", "Post Receive Failed(recv)", "socket", clientSocket);
                CleanUpSocket(clientSocket, work->sessionID);
            }
        }
    }

    bool IOCP::PostRecv(SOCKET clientSocket, uint64_t sessionID)
    {
        DWORD dwBytesReceived = 0;
        DWORD dwFlags = 0;

        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        if (!pOverlappedEx)
            return false;
        pOverlappedEx->op = IOOperation::RECV;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.resize(1);
        uint8_t* buf = nullptr;
        ClientContext* ownerContext = sessionManager->GetContext(clientSocket);
        if (!ownerContext) {
            Core::errorLogger->LogWarn("iocp", "PostRecv failed: ownerContext is null", "socket", clientSocket);
            overlappedExPool->Return(pOverlappedEx);
            return false;
        }
        uint16_t bufferLength = ownerContext->BeginRecvIO(sessionID, buf);
        if (bufferLength == 0) {
            Core::errorLogger->LogWarn("iocp", "can't allocate buffer", "socket", clientSocket);
            overlappedExPool->Return(pOverlappedEx);
            return false;
        }
        pOverlappedEx->ownerContext = ownerContext;
        pOverlappedEx->sessionID = sessionID;
        pOverlappedEx->wsaBuf[0].len = bufferLength;
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(buf);

        int result = WSARecv(clientSocket, &pOverlappedEx->wsaBuf[0], 1, &dwBytesReceived, &dwFlags, &pOverlappedEx->wsaOverlapped, NULL);

        if (result == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING) {
                Core::errorLogger->LogError("iocp", "WSAGetLastError ", "socket", clientSocket, "error message", std::to_string(err));
                ownerContext->CompleteRecvIO(sessionID);
                overlappedExPool->Return(pOverlappedEx);
                return false;
            }
        }
        return true;
    }
}