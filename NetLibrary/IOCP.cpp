#include "pch.h"
#include "IOCP.h"
#include <CoreLib/LoggerGlobal.h>
#include "OverlappedExPool.h"
#include "NetHandler.h"
#include "NetPerfCollector.h"
#include <charconv>

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
        uint16_t listenPort = LISTEN_PORT;
        char configuredPort[6]{};
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetEnvironmentVariableA("MMORPG_GAME_PORT", configuredPort, sizeof(configuredPort));
        if (length == 0 && GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
            Core::errorLogger->LogError("iocp", "MMORPG_GAME_PORT cannot be empty or unreadable.");
            return false;
        }
        if (length != 0) {
            unsigned parsedPort = 0;
            const auto parsed = std::from_chars(configuredPort, configuredPort + (std::min)(length, DWORD{5}), parsedPort);
            if (length >= sizeof(configuredPort) || parsed.ec != std::errc{} ||
                parsed.ptr != configuredPort + length || parsedPort == 0 || parsedPort > UINT16_MAX) {
                Core::errorLogger->LogError("iocp", "MMORPG_GAME_PORT must be an integer from 1 to 65535.");
                return false;
            }
            listenPort = static_cast<uint16_t>(parsedPort);
        }
        serverAddr.sin_port = htons(listenPort);

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

                const bool expectedShutdown = pOverlappedEx->op == IOOperation::ACCEPT &&
                    err == ERROR_OPERATION_ABORTED && !m_isRunning.load(std::memory_order_relaxed) &&
                    !m_receiving.load(std::memory_order_relaxed);
                if (!expectedShutdown)
                    Core::errorLogger->LogWarn("iocp", "GetQueuedCompletionStatus failed", "error code", std::to_string(err), "socket", clientSocket);

                if (pOverlappedEx->op == IOOperation::ACCEPT) {
                    overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf[0].buf);
                    closesocket(clientSocket); // 세션 등록 전 소켓 — 직접 닫기
                    if (m_isRunning.load(std::memory_order_relaxed))
                        PostAccept();          // accept 슬롯 보충
                }
                else {
                    CleanUpSocket(clientSocket, pOverlappedEx->sessionID); // RECV/SEND: 등록된 세션 정리
                }
                if (pOverlappedEx->op == IOOperation::RECV && pOverlappedEx->ownerContext != nullptr)
                    pOverlappedEx->ownerContext->CompleteRecvIO(pOverlappedEx->sessionID);
                overlappedExPool->Return(pOverlappedEx);
                continue;
            }


            switch (pOverlappedEx->op) {
            case IOOperation::RECV:
                HandleRecvCompletion(pOverlappedEx, bytesTransferred, index);
                break;
            case IOOperation::SEND:
                HandleSendCompletion(pOverlappedEx, bytesTransferred);
                // SEND는 재등록하거나 자체 반납하므로 공통 반납을 건너뛴다.
                continue;
            case IOOperation::ACCEPT:
                HandleAcceptCompletion(pOverlappedEx);
                break;
            default:
                break;
            }
            if (pOverlappedEx->op == IOOperation::RECV && pOverlappedEx->ownerContext != nullptr)
                pOverlappedEx->ownerContext->CompleteRecvIO(pOverlappedEx->sessionID);
            overlappedExPool->Return(pOverlappedEx);
        }
    }

    void IOCP::CleanUpSocket(SOCKET clientSocket)
    {
        if (netHandler->OnDisConnect(clientSocket))
        {
            Core::sysLogger->LogInfo("iocp", "Disconnect", "socket", clientSocket);
            if (!CancelIoEx((HANDLE)clientSocket, nullptr)) {
                int err = GetLastError();
                Core::errorLogger->LogWarn("iocp", "CancelIoEx failed", "socket", clientSocket, "error", err);
            }
            closesocket(clientSocket);
        }
    }

    void IOCP::CleanUpSocket(SOCKET clientSocket, uint64_t expectedSessionID)
    {
        if (netHandler->OnDisConnect(clientSocket, expectedSessionID))
        {
            // 여러 스레드에서 동시에 호출될 수 있지만,
            // NetHandler가 Disconnect를 단일 책임으로 관리하여 멱등성이 보장됨.
            // 중복으로 CleanUp 요청이 와도 최초 1회만 true를 반환함.
            Core::sysLogger->LogInfo("iocp", "Disconnect", "socket", clientSocket);
            if (!CancelIoEx((HANDLE)clientSocket, nullptr)) {
                int err = GetLastError();
                Core::errorLogger->LogWarn("iocp", "CancelIoEx failed", "socket", clientSocket, "error", err);
            } // 취소 completion이 회수될 때까지 RECV Context는 pendingIOCnt로 유지
            closesocket(clientSocket);
        }
    }

    void IOCP::AbortSocket(SOCKET clientSocket, uint64_t expectedSessionID) {
        if (clientSocket == INVALID_SOCKET)
            return;
        Core::sysLogger->LogInfo("iocp", "Abort Socket", "socket", clientSocket);
        CleanUpSocket(clientSocket, expectedSessionID);
    }
}
