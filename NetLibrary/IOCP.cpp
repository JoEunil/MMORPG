#include "pch.h"
#include "IOCP.h"
#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/IPacket.h>
#include "OverlappedExPool.h"
#include "NetHandler.h"
#include "Packet.h"
#include "SessionManager.h"
#include "NetPerfCollector.h"

namespace Net {
    void IOCP::Start()
    {
        if (!CreateListenSocket()) {
            return;
        }
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        // IOCP 핸들 생성

        if (CreateIoCompletionPort((HANDLE)m_listenSock, m_hIOCP, (ULONG_PTR)m_listenSock, 0) == NULL) {
            Core::errorLogger->LogError("iocp", "Failed to associate listen socket with IOCP");
            return;
        }
        // 서버 소켓을 IOCP 큐에 등록 (AcceptEx)

        GUID guidAcceptEx = WSAID_ACCEPTEX;
        DWORD bytes = 0;

        int result = WSAIoctl(
            m_listenSock,
            SIO_GET_EXTENSION_FUNCTION_POINTER, // 제어 명령 ( 함수 포인터 가져오기)
            &guidAcceptEx,// 어떤 함수인지
            sizeof(guidAcceptEx),
            &m_lpfnAcceptEx,// 받아올 포인터
            sizeof(m_lpfnAcceptEx),
            &bytes,
            NULL,
            NULL
        );
        // AcceptEx 함수 포인터를 가져오기
        if (result == SOCKET_ERROR) {
            Core::errorLogger->LogError("iocp", "AcceptEx ioctl failed");
            return;
        }

        if (!CreateWorkerThread()) {
            Core::errorLogger->LogError("iocp", "Worker thread creation failed.");
            return;
        }

        for (int i = 0; i < PREPOSTED_ACCEPTS; i++)
        {
            PostAccept();
        }
        m_receiving.store(true, std::memory_order_relaxed);
    }

    bool IOCP::CreateListenSocket()
    {
        // 서버 소켓 생성
        m_listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (m_listenSock == INVALID_SOCKET) {
            Core::errorLogger->LogError("iocp", "Create Listen socket Failed");
            return false;
        }
        // 서버 소켓 바인딩
        sockaddr_in serverAddr;
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, LISTEN_ADDRESS, &serverAddr.sin_addr);
        serverAddr.sin_port = htons((UINT16)LISTEN_PORT);

        if (bind(m_listenSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            Core::errorLogger->LogError("iocp", "Server socket bind failed.");
            return false;
        }

        Core::sysLogger->LogInfo("iocp", "Server socket bind success.");
        // 서버 소켓 리스닝
        if (listen(m_listenSock, SOMAXCONN) == SOCKET_ERROR) {
            Core::errorLogger->LogError("iocp", "Server socket listen failed.");
            return false;
        }
        Core::sysLogger->LogInfo("iocp", "Server socket listen success.");
        return true;
    }
    //Waiting Thread Queue에서 대기할 쓰레드들 생성
    bool IOCP::CreateWorkerThread() {
        m_isRunning.store(true, std::memory_order_relaxed);
        m_threads.resize(IOCP_THREADPOOL_SIZE);
        static const int kIocpCores[] = { 1, 3, 5, 7, 8, 9, 10, 11 };
        constexpr int kIocpCoreCount = sizeof(kIocpCores) / sizeof(kIocpCores[0]);
        for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&IOCP::WorkerThreadFunc, this, i);
            HANDLE h = (HANDLE)m_threads[i].native_handle();
            int core = kIocpCores[i % kIocpCoreCount];
            if (SetThreadAffinityMask(h, (DWORD_PTR)1 << core) == 0)
                Core::errorLogger->LogWarn("iocp", "SetThreadAffinityMask failed", "worker", i, "core", core, "err", (uint64_t)GetLastError());
            else
                Core::sysLogger->LogInfo("iocp", "worker pinned", "worker", i, "core", core);
        }
        return true;
    }

    void IOCP::CleanUp()
    {
        if (!m_isRunning.exchange(false, std::memory_order_relaxed))
            return;
        Core::sysLogger->LogInfo("iocp", "IOCP CleanUp");

        // 소켓 리소스 해제
        if (m_listenSock != INVALID_SOCKET) {
            closesocket(m_listenSock);
        }

        // 워커 스레드 수 만큼 더미 작업을 보냄
        for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++) {
            if (!PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr)) {
                Core::errorLogger->LogError("iocp", "Failed to post dummy completion status.", "thread index", i);
            }
        }

        for (auto& thread : m_threads) {
            if (thread.joinable()) {
                //joinable == false -> already joined or detatched
                thread.join();
            }
            Core::sysLogger->LogInfo("iocp", "iocp worker threads stopped");
        }
        // IOCP 객체 닫기
        if (m_hIOCP != NULL) {
            CloseHandle(m_hIOCP);
        }
    }

    void IOCP::WorkerThreadFunc(const int index)
    {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        Core::sysLogger->LogInfo("iocp", "iocp worker thread started", "threadID", ss.str(), "thread index", index);

        int current_thread = GetCurrentThreadId();
        while (m_isRunning.load(std::memory_order_relaxed))
        {
            DWORD bytesTransferred;
            ULONG_PTR completionKey;  // socket
            LPOVERLAPPED pOverlapped;

            BOOL result = GetQueuedCompletionStatus(m_hIOCP,
                &bytesTransferred, //실제로 전송된 바이트
                &completionKey,
                &pOverlapped,
                INFINITE // 대기할시간
            );
            // 블로킹 함수라서 작업이 들어올 떄까지 기다린다.
            if (result && pOverlapped == nullptr)
            {
                Core::sysLogger->LogInfo("iocp", "Close Signa Received");
                // IOCP 종료 신호
                break;
            }

            if (pOverlapped == nullptr)
            {
                Core::errorLogger->LogError("iocp", "GQCS failed without completion", "error", GetLastError());
                // GQCS 호출 자체가 실패해서 완료를 하나도 꺼내지 못한 경우
                // 포트가 닫힌 경우여서 종료 처리
                fatalError->store(true, std::memory_order_relaxed);
                cv->notify_one();
                break;
            }

            STOverlappedEx* pOverlappedEx = reinterpret_cast<STOverlappedEx*>(pOverlapped);
            WSABUF pBuffer = pOverlappedEx->wsaBuf[0];
            SOCKET clientSocket = pOverlappedEx->clientSocket;

            if (result == FALSE)
            {
                DWORD err = GetLastError();

                Core::errorLogger->LogWarn("iocp", "GetQueuedCompletionStatus failed", "error code", std::to_string(err), "socket", clientSocket);

                if (pOverlappedEx->op == IOOperation::ACCEPT) {
                    overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf[0].buf);
                    closesocket(clientSocket); // 세션 등록 전 소켓 — 직접 닫기
                    PostAccept();              // accept 슬롯 보충
                }
                else {
                    CleanUpSocket(clientSocket); // RECV/SEND: 등록된 세션 정리
                }
                overlappedExPool->Return(pOverlappedEx);
                continue;
            }


            switch (pOverlappedEx->op) {
            case IOOperation::RECV:
                if (!m_receiving.load(std::memory_order_relaxed))
                    break;
                if (bytesTransferred == 0) {
                    Core::sysLogger->LogInfo("iocp", "Client FIN received", "socket", clientSocket);
                    CleanUpSocket(clientSocket);
                    break;
                }
                perfCollector->AddRecvCnt(index);
                netHandler->OnRecv(clientSocket, reinterpret_cast<uint8_t*>(pOverlappedEx->wsaBuf[0].buf), bytesTransferred);

                if (!PostRecv(clientSocket)) {
                    Core::errorLogger->LogWarn("iocp", "Post Receive Failed(recv)", "socket", clientSocket);
                    CleanUpSocket(clientSocket);
                }
                break;
            case IOOperation::SEND: {
                pOverlappedEx->sentBytes += bytesTransferred;
                if (pOverlappedEx->sentBytes < pOverlappedEx->totalBytes) {
                    ResumeSend(pOverlappedEx); // 커널이 다시 소유 -> 반납 금지
                    continue; // 하단 공용 Return 을 건너뜀
                    // 소유권 측면에서 smart pointer나 flag 패턴이 여기에는 맞지 않음. 
                    // smart pointer:  RAII는 lifetime == scope일 때 가치가 있는데, 
                    // OVERLAPPED의 수명은 post~completion scope 밖(pending IO)에 결박됨.
                    // flag: 소유권을 데이터로 복제하는 것, 전형적인 안티 패턴
                } else {
                    auto next = sessionManager->DequeueSend(clientSocket);
                    if (next)
                        DoWSASend(next);
                }
                break;
            }
            case IOOperation::ACCEPT: {
                // completion key는 listen 소켓
                if (!m_receiving.load(std::memory_order_relaxed))
                    break;
                overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf[0].buf);
                setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&m_listenSock, sizeof(m_listenSock));
                // NAGLE 알고리즘 OFF
                BOOL nodelay = TRUE;
                setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

                if (CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP, (ULONG_PTR)clientSocket, 0) == nullptr) {
                    closesocket(clientSocket);
                    break;
                };

                if (!netHandler->OnAccept(clientSocket)) {
                    closesocket(clientSocket);
                    PostAccept();
                    break;
                }

                if (!PostRecv(clientSocket)) {
                    Core::errorLogger->LogWarn("iocp", "Post Receive Failed(accept)", "socket", clientSocket);
                    CleanUpSocket(clientSocket);
                }
                PostAccept();
                break;
            }
            default:
                break;
            }
            overlappedExPool->Return(pOverlappedEx);
        }
    }

    void IOCP::PostAccept()
    {
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
                Core::errorLogger->LogError("iocp", "Critical: failed to post AcceptEx", "error", err);

                m_isRunning.store(false, std::memory_order_relaxed);
                fatalError->store(true, std::memory_order_relaxed);
                cv->notify_one();

                overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf[0].buf);
                overlappedExPool->Return(pOverlappedEx);
                closesocket(clientSocket);
            }
        }
    }

    bool IOCP::PostRecv(SOCKET clientSocket)
    {
        DWORD dwBytesReceived = 0;
        DWORD dwFlags = 0;

        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        if (!pOverlappedEx)
            return false;
        pOverlappedEx->op = IOOperation::RECV;
        pOverlappedEx->clientSocket = clientSocket;
        uint8_t* buf = nullptr;
        pOverlappedEx->wsaBuf.resize(1);
        pOverlappedEx->wsaBuf[0].len = netHandler->AllocateBuffer(clientSocket, buf);
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(buf);
        if (pOverlappedEx->wsaBuf[0].len == 0) {
            Core::errorLogger->LogWarn("iocp", "can't allocate buffer", "socket", clientSocket);
            overlappedExPool->Return(pOverlappedEx);
            return false;
        }

        int result = WSARecv(clientSocket, &pOverlappedEx->wsaBuf[0], 1, &dwBytesReceived, &dwFlags, &pOverlappedEx->wsaOverlapped, NULL);

        if (result == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING) {
                Core::errorLogger->LogError("iocp", "WSAGetLastError ", "socket", clientSocket, "error message", std::to_string(err));
                overlappedExPool->Return(pOverlappedEx);
                return false;
            }
        }
        return true;
    }

    void IOCP::CleanUpSocket(SOCKET clientSocket)
    {
        if (netHandler->OnDisConnect(clientSocket))
        {
            // 여러 스레드에서 동시에 호출될 수 있지만,
            // NetHandler가 Disconnect를 단일 책임으로 관리하여 멱등성이 보장됨.
            // 중복으로 CleanUp 요청이 와도 최초 1회만 true를 반환함.
            Core::sysLogger->LogInfo("iocp", "Disconnect", "socket", clientSocket);
            if (!CancelIoEx((HANDLE)clientSocket, nullptr)) {
                int err = GetLastError();
                Core::errorLogger->LogWarn("iocp", "CancelIoEx failed", "socket", clientSocket, "error", err);
            } // pending IO를 즉시 취소
            closesocket(clientSocket);
        }
    }


    void IOCP::SendData(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet)
    {
        SOCKET clientSocket = sessionManager->GetSocket(sessionID);
        if (clientSocket == INVALID_SOCKET) {
            //Core::errorLogger->LogWarn("iocp", "try send to INVALID SOCKET", "session" , sessionID); 
            // 정상 실행 흐름에서 발생 가능하고, 발생 빈도가 매우 높다.
            return;
        }
        DWORD dwBytesSent = 0;
        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        if (!pOverlappedEx)
            return ;
        pOverlappedEx->op = IOOperation::SEND;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.resize(1);
        pOverlappedEx->wsaBuf[0].len =  packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->sharedPacket = packet;
        EnqueueSendResult status = sessionManager->EnqueueSend(clientSocket, pOverlappedEx);
        if  (status == EnqueueSendResult::Ready) {
            DoWSASend(pOverlappedEx);
        }
        else if (status == EnqueueSendResult::Failed) {
            overlappedExPool->Return(pOverlappedEx);
        }
        else if (status == EnqueueSendResult::QueueFull) {
            Core::errorLogger->LogWarn("iocp", "send queue full, packet dropped", "session", sessionID);
            overlappedExPool->Return(pOverlappedEx);
        }
    }


    void IOCP::SendDataChunks(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet, std::vector<std::shared_ptr<Core::IPacket>>& packetChunks)
    {
        SOCKET clientSocket = sessionManager->GetSocket(sessionID);
        if (clientSocket == INVALID_SOCKET) {
            //Core::errorLogger->LogWarn("iocp", "try send to INVALID SOCKET", "session" , sessionID); 
            // 정상 실행 흐름에서 발생 가능하고, 발생 빈도가 매우 높다.
            return;
        }
        DWORD dwBytesSent = 0;
        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        if (!pOverlappedEx)
            return;
        pOverlappedEx->op = IOOperation::SEND;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.resize(1);
        pOverlappedEx->wsaBuf[0].len = packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->sharedPacket = packet;
        pOverlappedEx->packetChunks = packetChunks;
        for (auto& chunk : packetChunks)
        {
            pOverlappedEx->wsaBuf.emplace_back(WSABUF{ chunk->GetLength(), reinterpret_cast<char*>(chunk->GetBuffer()) });
        }
        EnqueueSendResult status = sessionManager->EnqueueSend(clientSocket, pOverlappedEx);
        if (status == EnqueueSendResult::Ready) {
            DoWSASend(pOverlappedEx);
        }
         else if (status == EnqueueSendResult::Failed) {
             overlappedExPool->Return(pOverlappedEx);
        }
         else if (status == EnqueueSendResult::QueueFull) {
             Core::errorLogger->LogWarn("iocp", "send queue full, packet dropped", "session", sessionID);
             overlappedExPool->Return(pOverlappedEx);
        }

    }

    void IOCP::SendDataUnique(uint64_t sessionID, std::unique_ptr<Core::IPacket, Core::PacketDeleter> packet)
    {
        SOCKET clientSocket = sessionManager->GetSocket(sessionID);
        if (clientSocket == INVALID_SOCKET) {
            //Core::errorLogger->LogWarn("iocp", "try send to INVALID SOCKET", "session", sessionID);
            return;
        }
        DWORD dwBytesSent = 0;
        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        if (!pOverlappedEx)
            return ;
        pOverlappedEx->op = IOOperation::SEND;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.resize(1);
        pOverlappedEx->wsaBuf[0].len = packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->uniquePacket = std::move(packet);
        EnqueueSendResult status = sessionManager->EnqueueSend(clientSocket, pOverlappedEx);
        if (status == EnqueueSendResult::Ready) {
            DoWSASend(pOverlappedEx);
        }
         else if (status == EnqueueSendResult::Failed) {
             overlappedExPool->Return(pOverlappedEx);
        }
         else if (status == EnqueueSendResult::QueueFull) {
            Core::errorLogger->LogWarn("iocp", "send queue full, packet dropped", "session", sessionID);
            overlappedExPool->Return(pOverlappedEx);
        }
    }
    void IOCP::DoWSASend(STOverlappedEx* pOverlappedEx) {
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
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING)
            {
                SOCKET sock = pOverlappedEx->clientSocket;
                overlappedExPool->Return(pOverlappedEx);
                CleanUpSocket(sock);
                Core::errorLogger->LogWarn("iocp", "WSASend failed: ", "socket", pOverlappedEx->clientSocket, "error message", std::to_string(err));
            }
        }
    }
    void IOCP::ResumeSend(STOverlappedEx* pOverlappedEx) {
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
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING)
            {
                SOCKET sock = pOverlappedEx->clientSocket;
                overlappedExPool->Return(pOverlappedEx);
                CleanUpSocket(sock);
                Core::errorLogger->LogWarn("iocp", "WSASend failed: ", "socket", sock, "error message", std::to_string(err));
            }
        }
    }

    void IOCP::AbortSocket(SOCKET clientSocket) {
        if (clientSocket == INVALID_SOCKET)
            return;
        Core::sysLogger->LogInfo("iocp", "Abort Socket", "socket", clientSocket);
        CleanUpSocket(clientSocket); 
    }
}
