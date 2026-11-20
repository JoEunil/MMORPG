#include "pch.h"
#include "ServerResponseWorker.h"

#include <optional>
#include "../Interface/INetworkSender.h"
#include "../World/Interface/IZoneRequestQueue.h"
#include "../LoggerGlobal.h"

namespace Core {
    struct ServerResponseWorker::Work {
        struct BootstrapRequest {
            uint16_t zoneID;
            uint64_t ownershipEpoch;
        };
        uint64_t sessionID;
        std::unique_ptr<IPacket, PacketDeleter> packet;
        std::optional<BootstrapRequest> bootstrap;
        Work* next = nullptr;
    };

    ServerResponseWorker::ServerResponseWorker() = default;
    ServerResponseWorker::~ServerResponseWorker() {
        Stop();
    }

    void ServerResponseWorker::Initialize(INetworkSender& sender, IZoneRequestQueue& requests) {
        networkSender = &sender;
        zoneRequests = &requests;
    }

    bool ServerResponseWorker::IsReady() const {
        if (!networkSender) {
            sysLogger->LogError("server response", "network sender not initialized");
            return false;
        }
        if (!zoneRequests) {
            sysLogger->LogError("server response", "zone request queue not initialized");
            return false;
        }
        if (!m_running.load() || !m_thread.joinable()) {
            sysLogger->LogError("server response", "worker not running");
            return false;
        }
        return true;
    }

    void ServerResponseWorker::Start() {
        m_running.store(true);
        m_thread = std::thread(&ServerResponseWorker::ThreadFunc, this);
    }

    void ServerResponseWorker::Stop() {
        if (!m_running.exchange(false))
            return;
        // Finish admitted pushes before closing the draining semaphore.
        auto active = m_enqueuing.load();
        while (active != 0) {
            m_enqueuing.wait(active);
            active = m_enqueuing.load();
        }
        m_available.Close();
        if (m_thread.joinable())
            m_thread.join();
    }

    bool ServerResponseWorker::EnqueueWork(std::unique_ptr<Work> work) {
        const auto sessionID = work->sessionID;
        if (!work->packet) {
            errorLogger->LogWarn("server response", "response packet unavailable", "sessionID", sessionID);
            return false;
        }
        m_enqueuing.fetch_add(1);
        const bool accepted = m_running.load();
        if (accepted) {
            auto* entry = work.release();
            auto* head = m_pending.load(std::memory_order_relaxed);
            do {
                entry->next = head;
            } while (!m_pending.compare_exchange_weak(head, entry, std::memory_order_release, std::memory_order_relaxed));
            m_available.Release();
        }
        if (m_enqueuing.fetch_sub(1) == 1)
            m_enqueuing.notify_all();
        if (!accepted)
            errorLogger->LogWarn("server response", "response queue closed", "sessionID", sessionID);
        return accepted;
    }

    void ServerResponseWorker::ThreadFunc() {
        sysLogger->LogInfo("server response", "worker started");
        while (m_available.Acquire()) {
            if (m_ready == nullptr) {
                auto* entry = m_pending.exchange(nullptr, std::memory_order_acquire);
                // Reverse the published stack once per batch to preserve enqueue order.
                while (entry != nullptr) {
                    auto* next = entry->next;
                    entry->next = m_ready;
                    m_ready = entry;
                    entry = next;
                }
            }
            const std::unique_ptr<Work> work(m_ready);
            m_ready = work->next;
            Process(*work);
        }
        sysLogger->LogInfo("server response", "worker stopped");
    }

    void ServerResponseWorker::Process(Work& work) {
        networkSender->SendDataUnique(work.sessionID, std::move(work.packet));
        if (work.bootstrap) {
            // Capture bootstrap on the owning Zone only after the ACK is submitted.
            const auto& request = *work.bootstrap;
            if (!zoneRequests->TryEnqueueBootstrap(request.zoneID, work.sessionID, request.ownershipEpoch, false))
                errorLogger->LogWarn("server response", "bootstrap request rejected", "sessionID", work.sessionID, "zoneID", request.zoneID);
        }
    }

    bool ServerResponseWorker::Enqueue(uint64_t sessionID, std::unique_ptr<IPacket, PacketDeleter> packet) {
        return EnqueueWork(std::make_unique<Work>(Work{sessionID, std::move(packet), std::nullopt}));
    }

    bool ServerResponseWorker::EnqueueZoneChange(uint64_t sessionID, std::unique_ptr<IPacket, PacketDeleter> packet, uint16_t zoneID, uint64_t ownershipEpoch) {
        return EnqueueWork(std::make_unique<Work>(Work{sessionID, std::move(packet), Work::BootstrapRequest{zoneID, ownershipEpoch}}));
    }
}
