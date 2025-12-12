#include "pch.h"
#include "IOCP.h"
#include <CoreLib/ILogger.h>
#include <CoreLib/IPacket.h>
#include "OverlappedExPool.h"
#include "NetHandler.h"
#include "Packet.h"
#include "SessionManager.h"

namespace Net {
    void IOCP::Start()
    {
        if (!CreateListenSocket()) {
            logger->LogError("Create Listen socket Failed");
            return;
        }
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        // IOCP 핸들 생성

        CreateIoCompletionPort((HANDLE)m_listenSock, m_hIOCP, (ULONG_PTR)m_listenSock, 0);
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
            logger->LogError("AcceptEx ioctl failed");
            return;
        }
        m_isRunning.store(true);

        if (!CreateWorkerThread()) {
            logger->LogError("Worker thread creation failed.");
            return;
        }

        for (int i = 0; i < PREPOSTED_ACCEPTS; i++)
        {
            PostAccept();
        }
        m_receiving.store(true);
    }

    bool IOCP::CreateListenSocket()
    {
        // 서버 소켓 생성
        m_listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (m_listenSock == INVALID_SOCKET) {
            logger->LogError("Server socket creation failed.");
            return false;
        }
        // 서버 소켓 바인딩
        sockaddr_in serverAddr;
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, LISTEN_ADDRESS, &serverAddr.sin_addr);
        serverAddr.sin_port = htons((UINT16)LISTEN_PORT);

        if (bind(m_listenSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            logger->LogError("Server socket bind failed.");
            return false;
        }

        logger->LogInfo("Server socket bind success.");
        // 서버 소켓 리스닝
        if (listen(m_listenSock, SOMAXCONN) == SOCKET_ERROR) {
            logger->LogError("Server socket listen failed.");
            return false;
        }
        logger->LogInfo("Server socket listen success.");
        return true;
    }
    //Waiting Thread Queue에서 대기할 쓰레드들 생성
    bool IOCP::CreateWorkerThread() {
        m_threads.resize(IOCP_THREADPOOL_SIZE);
        for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&IOCP::WorkerThreadFunc, this);
            HANDLE h = (HANDLE)m_threads[i].native_handle();
            // zone 스레드보다는 우선순위 낮고, 다른 스레드풀 보다는 우선순위 높게
            if (!::SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL)) {
                logger->LogError("Failed to set priority, zone {} thread" + std::to_string(i + 1));
            }
        }
        return true;
    }

    void IOCP::CleanUp()
    {
        m_isRunning.store(false);
        logger->LogError("IOCP CleanUp");

        // 소켓 리소스 해제
        if (m_listenSock != INVALID_SOCKET) {
            closesocket(m_listenSock);
        }

        // 워커 스레드 수 만큼 더미 작업을 보냄
        for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++) {
            if (!PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr)) {
                logger->LogError("Failed to post dummy completion status.");
            }
        }

        for (auto& thread : m_threads) {
            if (thread.joinable()) {
                //joinable == false -> already joined or detatched
                thread.join();
            }
        }
        // IOCP 객체 닫기
        if (m_hIOCP != NULL) {
            CloseHandle(m_hIOCP);
        }
    }

    void IOCP::WorkerThreadFunc()
    {
        int current_thread = GetCurrentThreadId();
        while (m_isRunning.load())
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
                // IOCP 종료 신호
                logger->LogError("Close Signal Received ");
                break;
            }
            if (result == FALSE)
            {
                DWORD err = GetLastError();
                overlappedExPool->Return(reinterpret_cast<STOverlappedEx*>(pOverlapped));
                logger->LogError("GetQueuedCompletionStatus failed with error code: " + std::to_string(err));
                continue;
            }

            STOverlappedEx* pOverlappedEx = reinterpret_cast<STOverlappedEx*>(pOverlapped);
            WSABUF pBuffer = pOverlappedEx->wsaBuf;
            SOCKET clientSocket = pOverlappedEx ->clientSocket;

            switch (pOverlappedEx->op) {
            case IOOperation::RECV:
                if (!m_receiving.load())
                    break;
                if (bytesTransferred == 0) {
                    logger->LogWarn(std::format("bytesTransferred == 0, socket: {}", clientSocket));
                    CleanUpSocket(clientSocket);
                    break;
                }

                netHandler->OnRecv(clientSocket, reinterpret_cast<uint8_t*>(pOverlappedEx->wsaBuf.buf), bytesTransferred);

                if (!PostRecv(clientSocket)) {
                    logger->LogWarn("Post Receive Failed");
                    CleanUpSocket(clientSocket);
                }
                break;
            case IOOperation::SEND:
                pOverlappedEx->sharedPacket.reset(); // 참조 카운트 1감소
                break;
            case IOOperation::ACCEPT:
                logger->LogInfo(std::format("Accept {}", clientSocket));
                // completion key는 listen 소켓
                if (!m_receiving.load())
                    break;
                overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf.buf);
                setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&m_listenSock, sizeof(m_listenSock));
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
                    logger->LogWarn("Post Receive Failed(accept)");
                    CleanUpSocket(clientSocket);
                }
                PostAccept();
                break;
            default:
                break;
            }
            overlappedExPool->Return(pOverlappedEx);
        }
        std::cout << "iocp thread stopped \n";
    }

    void IOCP::PostAccept()
    {
        SOCKET clientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (clientSocket == INVALID_SOCKET) {
            logger->LogError("Create client socket failed");
            return;
        }

        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        ZeroMemory(pOverlappedEx, sizeof(STOverlappedEx));
        pOverlappedEx->op = IOOperation::ACCEPT;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.buf = overlappedExPool->AcquireAcceptBuffer();

        DWORD bytesReceived = 0;
        BOOL bRet = m_lpfnAcceptEx(
            m_listenSock,
            clientSocket,
            pOverlappedEx->wsaBuf.buf,
            0,
            sizeof(SOCKADDR_IN) + 16,
            sizeof(SOCKADDR_IN) + 16,
            &bytesReceived,
            (LPOVERLAPPED)pOverlappedEx
        );

        if (bRet == FALSE) {
            int err = WSAGetLastError();
            if (err != ERROR_IO_PENDING) {
                //logger->LogError("Critical: failed to post AcceptEx. Shutting down server." + std::to_string(err));

                //m_isRunning.store(false);
                //fatalError->store(true);
                //cv->notify_one();

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
        pOverlappedEx->op = IOOperation::RECV;
        pOverlappedEx->clientSocket = clientSocket;
        uint8_t* buf = nullptr;
        pOverlappedEx->wsaBuf.len = netHandler->AllocateBuffer(clientSocket, buf);
        pOverlappedEx->wsaBuf.buf = reinterpret_cast<char*>(buf);
        if (pOverlappedEx->wsaBuf.len == 0) {
            logger->LogError("can't allocate buffer - socket:" + std::to_string(clientSocket));
            return false;
        }

        int result = WSARecv(clientSocket, &pOverlappedEx->wsaBuf, 1, &dwBytesReceived, &dwFlags, &pOverlappedEx->wsaOverlapped, NULL);

        if (result == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING) {
                logger->LogError("WSAGetLastError " + std::to_string(clientSocket) + " " + std::to_string(err));
                overlappedExPool->Return(pOverlappedEx);
                return false;
            }
        }
        return true;
    }

    void IOCP::CleanUpSocket(SOCKET clientSocket)
    {
        logger->LogInfo(std::format("Disconnect {}", clientSocket));
        netHandler->OnDisConnect(clientSocket); // session 제거 -> send 작업도 block
        CancelIoEx((HANDLE)clientSocket, nullptr); // pending IO를 즉시 취소
        closesocket(clientSocket);
    }


    void IOCP::SendData(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet)
    {
        SOCKET clientSocket = SessionManager::Instance().GetSocket(sessionID);
        if (clientSocket == INVALID_SOCKET) {
            logger->LogError("trye send to INVALID SOCKET");
            return;
        }
        DWORD dwBytesSent = 0;
        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        pOverlappedEx->op = IOOperation::SEND;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.buf = reinterpret_cast<char*>(packet->GetBuffer()); // 다른 타입이지만, 메모리 표현은 동일 -> reinter cast
        pOverlappedEx->wsaBuf.len = packet->GetLength();
        pOverlappedEx->sharedPacket = std::static_pointer_cast<Packet>(packet);
        // 상속관계 타입 변환은 static cast, 컴파일 타임에 변환되어 런타임에 비용 0
        int result = WSASend(clientSocket, &pOverlappedEx->wsaBuf, 1, &dwBytesSent, 0, &pOverlappedEx->wsaOverlapped, NULL);
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING)
            {
                overlappedExPool->Return(pOverlappedEx);
                logger->LogError("WSASend failed: " + std::to_string(clientSocket) + " "  + std::to_string(err));
            }
        }

    }
}
