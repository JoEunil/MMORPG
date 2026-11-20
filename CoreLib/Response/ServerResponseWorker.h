#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <BaseLib/DrainableSemaphore.h>
#include "Interface/IServerResponseQueue.h"

namespace Core {
    class INetworkSender;
    class IZoneRequestQueue;

    class ServerResponseWorker : public IServerResponseQueue {
        struct Work;
        // Work nodes also form the FIFO backlog, so a committed ACK is not dropped on capacity.
        std::atomic<Work*> m_pending = nullptr;
        Work* m_ready = nullptr; // Consumer-only FIFO batch.
        Base::DrainableSemaphore<true> m_available;
        std::thread m_thread;
        std::atomic<bool> m_running = false;
        std::atomic<uint32_t> m_enqueuing = 0;
        INetworkSender* networkSender = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;

        void Initialize(INetworkSender& sender, IZoneRequestQueue& requests);
        bool IsReady() const;
        void Start();
        void Stop();
        void ThreadFunc();
        void Process(Work& work);
        bool EnqueueWork(std::unique_ptr<Work> work);
        friend class Initializer;

      public:
        ServerResponseWorker();
        ~ServerResponseWorker();
        bool Enqueue(uint64_t sessionID, std::unique_ptr<IPacket, PacketDeleter> packet) override;
        bool EnqueueZoneChange(uint64_t sessionID, std::unique_ptr<IPacket, PacketDeleter> packet, uint16_t zoneID, uint64_t ownershipEpoch) override;
    };
}
