#pragma once
#include <cstddef>
#include <mutex>
#include <thread>
#include <atomic>
#include <new>
#include <unordered_map>
#include <memory>
#include <vector>

#include "ChatEvent.h"
#include "../LoggerGlobal.h"
#include "ChatSessionManager.h"
#include "../Config.h"
#include "NonSimulationConfig.h"
#include "../World/Interface/ISessionLifecycleListener.h"
#include <CoreLib/Interface/IPacket.h>
#include <BaseLib/NonBlockingBackPressureQueue.h>

namespace Core {
    const uint16_t CHAT_WORKER_SIZE = 1; // 변경하려면 ChatDestKey 활용해 샤딩 적용 필요.

    class INetworkSender;
    class PacketWriter;
    class CorePerfCollector;
    class ChatThreadPool : public ISessionLifecycleListener {
        ChatSessionManager m_sessions;
        alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> m_chatIdGenerater;
        char padding[std::hardware_destructive_interference_size - sizeof(uint64_t)];

        Base::NonBlockingBackPressureQueue<ChatEvent, CHAT_QUEUE_SIZE, CHAT_DEFER_QUEUE_SIZE> m_chatQueue;

        // NonBlockingBackPressureQueue는 primary/defer 전체 FIFO를 보장하지 않는다.
        // session별 최신 Zone sequence를 공유해 오래된 전환을 폐기한다.
        std::mutex m_zoneSequenceMutex;
        std::unordered_map<uint64_t, uint64_t> m_currentZoneSequence;

        std::vector<std::thread> m_workerThreadPool;
        std::atomic<bool> m_running;
        // 일단 싱글 스레드, Chat Filtering 등 CPU 부하 작업이 추가되면 스레드 수 늘리기.
        // 스레드 늘린다면 Group 단위로 샤딩. ChatDestKey 활용.
        // zone 단위로만 샤딩한다면, guild, party 채팅을 위해서 전체 zone에 동일 채팅을 처리해야됨.

        struct ChatDestKeyHash {
            // custom sturct라서 keyHash는 필요
            std::size_t operator()(const ChatDestKey& key) const noexcept {
                std::size_t h1 = std::hash<int>()(static_cast<int>(key.scope));
                std::size_t h2 = std::hash<uint64_t>()(key.id);
                return (h1 << 56) ^ h2;
                // scope가 값이 너무 작은 범위라서 해시 충돌 피하기 위해 최상위 8비트로 shift 후 xor
            }
        };

        struct EventVisitor {
            ChatThreadPool& pool;
            std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets;

            void operator()(const ChatSessionAddEvent& event) const;
            void operator()(const ChatZoneJoinEvent& event) const;
            void operator()(const ChatZoneLeaveEvent& event) const;
            void operator()(const ChatMessageEvent& event) const;
            void operator()(const ChatSessionDeleteEvent& event) const;
        };

        void Initialize(INetworkSender* sender, PacketWriter* w, CorePerfCollector* p) {
            networkSender = sender;
            writer = w;
            perfCollector = p;
            m_currentZoneSequence.reserve(MAX_USER_CAPACITY);
            m_chatIdGenerater.store(1, std::memory_order_relaxed);
        }

        bool IsReady() const {
            if (networkSender == nullptr) {
                sysLogger->LogError("chat thread", "iocp not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("chat thread", "writer not initialized");
                return false;
            }
            return true;
        }
        void Start() {
            m_running.store(true, std::memory_order_relaxed);
            for (int i = 0; i < CHAT_WORKER_SIZE; i++) {
                m_workerThreadPool.emplace_back(&ChatThreadPool::ThreadFunc, this);
            }
        }

        void Stop() {
            if (!m_running.exchange(false, std::memory_order_relaxed))
                return;
            for (auto& t : m_workerThreadPool) {
                if (t.joinable())
                    t.join();
            }
        }

        void ThreadFunc();
        bool ProcessQueuedEvent(std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void FlushChatBatches(std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void SendWhisper(const ChatMessageEvent& event, uint64_t chatID, uint32_t profileId);
        void SendPacketGroup(ChatDestKey key, std::shared_ptr<IPacket> packet);
        void ProcesChat(const ChatMessageEvent& curr, uint64_t chatID, uint32_t profileId,
            std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void AppendToChatBatch(const ChatMessageEvent& curr, uint64_t chatID, uint32_t profileId,
            std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        template<typename ZoneEvent>
        void ProcessZoneTransition(const ZoneEvent& event);
        void ApplyZoneTransition(const ChatZoneJoinEvent& event);
        void ApplyZoneTransition(const ChatZoneLeaveEvent& event);

        PacketWriter* writer = nullptr;
        INetworkSender* networkSender = nullptr;
        CorePerfCollector* perfCollector = nullptr;
        friend class Initializer;

      public:
        uint64_t OnSessionEnteredWorld(uint64_t sessionID, uint16_t zoneID, uint32_t profileID) override;
        void OnSessionZoneChanged(uint64_t sessionID, uint16_t previousZoneID, uint16_t currentZoneID) override;
        void OnSessionClosed(uint64_t sessionID, uint16_t zoneID) override;

      private:
        uint64_t AddChatSession(uint64_t session, uint32_t profileId) {
            uint64_t chatID = m_chatIdGenerater.fetch_add(1, std::memory_order_relaxed);

            {
                std::lock_guard<std::mutex> lock(m_zoneSequenceMutex);
                m_currentZoneSequence.insert_or_assign(session, 1);
            }

            ChatEvent e = ChatSessionAddEvent{session, chatID, profileId};
            EnqueueChat(e);
            return chatID;
        }

        void DeleteChatSession(uint64_t session) {
            ChatEvent e = ChatSessionDeleteEvent{session};
            EnqueueChat(e);
        }

      public:
        bool EnqueueChat(ChatEvent& event, Base::Priority priority = Base::Priority::Droppable) {
            if (!m_running.load(std::memory_order_relaxed))
                return false;

            return m_chatQueue.Enqueue(event, priority);
        }

      private:
        template<typename ZoneEvent>
        void EnqueueZoneTransition(ZoneEvent& event) {
            const uint64_t session = event.senderSessionID;
            const uint64_t zoneID = event.zoneID;

            std::lock_guard<std::mutex> lock(m_zoneSequenceMutex);
            auto it = m_currentZoneSequence.find(session);
            if (it == m_currentZoneSequence.end()) {
                errorLogger->LogError("chat thread", "zone transition before session add", "session", session, "zoneID", zoneID);
                return;
            }

            event.zoneSequence = it->second + 1;

            ChatEvent queued = event;
            if (EnqueueChat(queued, Base::Priority::Important)) {
                it->second = event.zoneSequence;
                return;
            }

            errorLogger->LogError("chat thread", "zone transition queue overflow", "session", session, "zoneID", zoneID);
        }

        // EnqueueChat으로 같이 처리하면 코드가 복잡해짐
        void EnqueueZoneJoin(uint64_t session, uint16_t dest) {
            ChatZoneJoinEvent e{session, dest};
            EnqueueZoneTransition(e);
        }

        void EnqueueZoneLeave(uint64_t session, uint16_t zoneID) {
            ChatZoneLeaveEvent e{session, zoneID};
            EnqueueZoneTransition(e);
        }
    };
}
