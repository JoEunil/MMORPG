#include "pch.h"
#include "IOCP.h"
#include <CoreLib/LoggerGlobal.h>
#include "OverlappedExPool.h"
#include "SessionManager.h"
#include <CoreLib/IPacket.h>

namespace Net {
    void IOCP::HandleSendCompletion(STOverlappedEx* work, DWORD bytesTransferred) {
        work->sentBytes += bytesTransferred;
        if (work->sentBytes < work->totalBytes) {
            ContinueSend(work);
            return;
        }
        auto next = sessionManager->DequeueSend(work->clientSocket);
        if (next)
            BeginSend(next);
        overlappedExPool->Return(work);
    }

    void IOCP::PostSend(EnqueueSendResult status, STOverlappedEx* pOverlappedEx, uint64_t sessionID) {
        if (status == EnqueueSendResult::Ready) {
            BeginSend(pOverlappedEx);
        }
        else if (status == EnqueueSendResult::Failed) {
            overlappedExPool->Return(pOverlappedEx);
        }
        else if (status == EnqueueSendResult::QueueFull) {
            Core::errorLogger->LogWarn("iocp", "send queue full, packet dropped", "session", sessionID);
            overlappedExPool->Return(pOverlappedEx);
        }
    }

    STOverlappedEx* IOCP::PrepareSend(uint64_t sessionID) {
        SOCKET clientSocket = sessionManager->GetSocket(sessionID);
        if (clientSocket == INVALID_SOCKET) {
            return;
        }
        DWORD dwBytesSent = 0;
        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        if (!pOverlappedEx)
            return;
        pOverlappedEx->op = IOOperation::SEND;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->sessionID = sessionID;
        pOverlappedEx->wsaBuf.resize(1);
        return pOverlappedEx;
    }

    void IOCP::SendData(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet)
    {
		STOverlappedEx* pOverlappedEx = PrepareSend(sessionID);
        pOverlappedEx->wsaBuf[0].len = packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->sharedPacket = packet;
        EnqueueSendResult status = sessionManager->EnqueueSend(pOverlappedEx);
        PostSend(status, pOverlappedEx, sessionID);
    }

    void IOCP::SendDataChunks(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet, std::vector<std::shared_ptr<Core::IPacket>>& packetChunks)
    {
        STOverlappedEx* pOverlappedEx = PrepareSend(sessionID);
        pOverlappedEx->wsaBuf[0].len = packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->sharedPacket = packet;
        pOverlappedEx->packetChunks = packetChunks;
        for (auto& chunk : packetChunks)
        {
            pOverlappedEx->wsaBuf.emplace_back(chunk->GetLength(), reinterpret_cast<char*>(chunk->GetBuffer()));
        }
        EnqueueSendResult status = sessionManager->EnqueueSend(pOverlappedEx);
        PostSend(status, pOverlappedEx, sessionID);
    }

    void IOCP::SendDataUnique(uint64_t sessionID, std::unique_ptr<Core::IPacket, Core::PacketDeleter> packet)
    {
        STOverlappedEx* pOverlappedEx = PrepareSend(sessionID);
        pOverlappedEx->wsaBuf[0].len = packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->uniquePacket = std::move(packet);
        EnqueueSendResult status = sessionManager->EnqueueSend(pOverlappedEx);
        PostSend(status, pOverlappedEx, sessionID);
    }

    void IOCP::HandleSendResult(STOverlappedEx* pOverlappedEx, int result) {
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING)
            {
                SOCKET sock = pOverlappedEx->clientSocket;
                uint64_t sessionID = pOverlappedEx->sessionID;
                overlappedExPool->Return(pOverlappedEx);
                CleanUpSocket(sock, sessionID);
                Core::errorLogger->LogWarn("iocp", "WSASend failed: ", "socket", sock, "error message", std::to_string(err));
            }
        }
    }
    void IOCP::BeginSend(STOverlappedEx* pOverlappedEx) {
        // 최초의 Send에서만 호출
        if (pOverlappedEx == nullptr)
            return;
        ZeroMemory(&pOverlappedEx->wsaOverlapped, sizeof(WSAOVERLAPPED));
        pOverlappedEx->totalBytes = 0;
        pOverlappedEx->sentBytes = 0;
        pOverlappedEx->originalBufs = pOverlappedEx->wsaBuf;
        for (auto& buf : pOverlappedEx->wsaBuf)
            pOverlappedEx->totalBytes += buf.len;
        int result = WSASend(pOverlappedEx->clientSocket, pOverlappedEx->wsaBuf.data(), (DWORD)pOverlappedEx->wsaBuf.size(), 0, 0, &pOverlappedEx->wsaOverlapped, NULL);
        HandleSendResult(pOverlappedEx, result);
    }
    void IOCP::ContinueSend(STOverlappedEx* pOverlappedEx) {
        ZeroMemory(&pOverlappedEx->wsaOverlapped, sizeof(WSAOVERLAPPED));
        int remain = pOverlappedEx->sentBytes;
        pOverlappedEx->wsaBuf.clear();
        for (auto& buf : pOverlappedEx->originalBufs)
        {
            if (remain >= buf.len)
            {
                remain -= buf.len;
                continue;
            }

            WSABUF newBuf;
            newBuf.buf = buf.buf + remain;
            newBuf.len = buf.len - remain;

            pOverlappedEx->wsaBuf.push_back(newBuf);

            remain = 0;
        }
        int result = WSASend(pOverlappedEx->clientSocket, pOverlappedEx->wsaBuf.data(), (DWORD)pOverlappedEx->wsaBuf.size(), 0, 0, &pOverlappedEx->wsaOverlapped, NULL);
        HandleSendResult(pOverlappedEx, result);
    }

}