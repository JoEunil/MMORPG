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
        m_receiving.store(true);
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
        m_isRunning.store(true);
        m_threads.resize(IOCP_THREADPOOL_SIZE);
        for (int i = 0; i < IOCP_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&IOCP::WorkerThreadFunc, this, i);
            HANDLE h = (HANDLE)m_threads[i].native_handle();
            // zone 스레드보다는 우선순위 낮고, 다른 스레드풀 보다는 우선순위 높게
            if (!::SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL)) {
                Core::errorLogger->LogError("iocp", "Failed to set priority", "zone_id", i + 1);
            }
        }
        return true;
    }

    void IOCP::CleanUp()
    {
        m_isRunning.store(false);
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
                Core::sysLogger->LogInfo("iocp", "Close Signa Received");
                // IOCP 종료 신호
                break;
            }

            STOverlappedEx* pOverlappedEx = reinterpret_cast<STOverlappedEx*>(pOverlapped);
            WSABUF pBuffer = pOverlappedEx->wsaBuf[0];
            SOCKET clientSocket = pOverlappedEx->clientSocket;

            if (result == FALSE)
            {
                DWORD err = GetLastError();
                overlappedExPool->Return(reinterpret_cast<STOverlappedEx*>(pOverlapped));
                Core::errorLogger->LogWarn("iocp", "GetQueuedCompletionStatus failed", "error code", std::to_string(err), "socket", clientSocket);
                CleanUpSocket(clientSocket);
                continue;
            }


            switch (pOverlappedEx->op) {
            case IOOperation::RECV:
                if (!m_receiving.load())
                    break;
                if (bytesTransferred == 0) {
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
            case IOOperation::SEND:
                break;
            case IOOperation::ACCEPT:
                // completion key는 listen 소켓
                if (!m_receiving.load())
                    break;
                overlappedExPool->ReturnAcceptBuf(pOverlappedEx->wsaBuf[0].buf);
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
                    Core::errorLogger->LogWarn("iocp", "Post Receive Failed(accept)", "socket", clientSocket);
                    CleanUpSocket(clientSocket);
                }
                PostAccept();
                break;
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
        ZeroMemory(pOverlappedEx, sizeof(STOverlappedEx));
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

                m_isRunning.store(false);
                fatalError->store(true);
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
            // Race condition이 발생할 수 있는 지점이지만,
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
        // Send 중 socket close와의 race condition은 발생할 수 있음.
        // 그러나 WSASend 실패 / IO 취소는 정상적인 종료 경로
        // 중복 close나 메모리 손상은 발생하지 않음.
        // 이 경로에 mutex를 사용하면 성능에 영향이 커서 의도적으로 허용함.
        
        SOCKET clientSocket = sessionManager->GetSocket(sessionID);
        if (clientSocket == INVALID_SOCKET) {
            //Core::errorLogger->LogWarn("iocp", "try send to INVALID SOCKET", "session" , sessionID); 
            // 정상 실행 흐름에서 발생 가능하고, 발생 빈도가 매우 높다.
            return;
        }
        DWORD dwBytesSent = 0;
        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
        pOverlappedEx->op = IOOperation::SEND;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.resize(1);
        pOverlappedEx->wsaBuf[0].len =  packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->sharedPacket = packet;
        // 상속관계 타입 변환은 static cast, 컴파일 타임에 변환되어 런타임에 비용 0
        int result = WSASend(clientSocket, &pOverlappedEx->wsaBuf[0], 1,  &dwBytesSent, 0, &pOverlappedEx->wsaOverlapped, NULL);
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING)
            {
                overlappedExPool->Return(pOverlappedEx);
                Core::errorLogger->LogWarn("iocp", "WSASend failed: ","socket", clientSocket, "error message", std::to_string(err));
            }
        }

    }


    void IOCP::SendDataChunks(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet, std::vector<std::shared_ptr<Core::IPacket>>& packetChunks)
    {
        // Send 중 socket close와의 race condition은 발생할 수 있음.
        // 그러나 WSASend 실패 / IO 취소는 정상적인 종료 경로
        // 중복 close나 메모리 손상은 발생하지 않음.
        // 이 경로에 mutex를 사용하면 성능에 영향이 커서 의도적으로 허용함.

        SOCKET clientSocket = sessionManager->GetSocket(sessionID);
        if (clientSocket == INVALID_SOCKET) {
            //Core::errorLogger->LogWarn("iocp", "try send to INVALID SOCKET", "session" , sessionID); 
            // 정상 실행 흐름에서 발생 가능하고, 발생 빈도가 매우 높다.
            return;
        }
        DWORD dwBytesSent = 0;
        STOverlappedEx* pOverlappedEx = overlappedExPool->Acquire();
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
        // 상속관계 타입 변환은 static cast, 컴파일 타임에 변환되어 런타임에 비용 0
        int result = WSASend(clientSocket, pOverlappedEx->wsaBuf.data(), static_cast<DWORD>(pOverlappedEx->wsaBuf.size()),
            &dwBytesSent, 0, &pOverlappedEx->wsaOverlapped, NULL);
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING)
            {
                overlappedExPool->Return(pOverlappedEx);
                Core::errorLogger->LogWarn("iocp", "WSASend failed: ", "socket", clientSocket, "error message", std::to_string(err));
            }
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
        pOverlappedEx->op = IOOperation::SEND;
        pOverlappedEx->clientSocket = clientSocket;
        pOverlappedEx->wsaBuf.resize(1);
        pOverlappedEx->wsaBuf[0].len = packet->GetLength();
        pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
        pOverlappedEx->uniquePacket = std::move(packet);
        int result = WSASend(clientSocket, &pOverlappedEx->wsaBuf[0], 1, &dwBytesSent, 0, &pOverlappedEx->wsaOverlapped, NULL);
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING)
            {
                overlappedExPool->Return(pOverlappedEx);
                Core::errorLogger->LogWarn("iocp", "WSASend failed: ", "socket", clientSocket, "error message", std::to_string(err));
            }
        }

    }

    void IOCP::AbortSocket(SOCKET clientSocket) {
        if (clientSocket == INVALID_SOCKET)
            return;
        CleanUpSocket(clientSocket); 
    }
}
