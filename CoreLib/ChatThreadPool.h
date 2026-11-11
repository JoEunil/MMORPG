#pragma once
#include <mutex>
#include <thread>
#include <atomic>
#include <new>
#include <unordered_map>
#include <memory>
#include <vector>

#include "PacketTypes.h"
#include "LoggerGlobal.h"
#include "ChatSessionManager.h"
#include "Config.h"
#include <CoreLib/IPacket.h>
#include <BaseLib/NonBlockingBackPressureQueue.h>

namespace Core {
    enum class ChatEventType {
        SESSION_ADD,
        SESSION_DELETE,
        CHAT,
        ZONE_JOIN,
        ZONE_LEAVE,
    };

    struct ChatDestKey {
        CHAT_SCOPE scope;
        uint64_t id; // zone, party, guild, targetChatID 
        bool operator==(const ChatDestKey& other) const noexcept {
            return scope == other.scope && id == other.id;
        }
    };

    struct ChatEvent {
        ChatEventType type;
        uint64_t senderSessionID;
        uint64_t senderChatID;
        uint32_t senderProfileId; // SESSION_ADD에서만 사용
        uint64_t zoneSequence = 0; // ZONE_JOIN / ZONE_LEAVE에서만 사용
        ChatDestKey key;
        std::string message;
    };

    const uint16_t CHAT_WORKER_SIZE = 1;  // 변경하려면 ChatDestKey 활용해 샤딩 적용 필요. 

    class INetworkSender;
    class PacketWriter;
    class CorePerfCollector;
    class ChatThreadPool {
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
                return (h1<< 56) ^ h2 ;
                // scope가 값이 너무 작은 범위라서 해시 충돌 피하기 위해 최상위 8비트로 shift 후 xor
            }   
        };

        void Initialize(INetworkSender* sender, PacketWriter* w, CorePerfCollector* p) {
            networkSender = sender;
            writer = w;
            perfCollector = p;
            m_currentZoneSequence.reserve(MAX_USER_CAPACITY);
            m_chatIdGenerater.store(1, std::memory_order_relaxed);
        }

        bool IsReady() {
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
            for (int i = 0; i < CHAT_WORKER_SIZE; i++)
            {
                m_workerThreadPool.emplace_back(&ChatThreadPool::ThreadFunc, this);
            }
        }

        void Stop() {
            if (!m_running.exchange(false, std::memory_order_relaxed))
                return;
            for (auto& t : m_workerThreadPool)
            {
                if (t.joinable())
                    t.join();
            }
        }

        void ThreadFunc();
        bool ProcessQueuedEvent(std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void FlushChatBatches(std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void SendWhisper(const ChatEvent& event, uint64_t chatID, uint32_t profileId);
        void SendPacketGroup(ChatDestKey key, std::shared_ptr<IPacket> packet);
        void ProcesChat(ChatEvent& curr, uint64_t chatID, uint32_t profileId,
            std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void AppendToChatBatch(const ChatEvent& curr, uint64_t chatID, uint32_t profileId,
            std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void ProcessZoneTransition(ChatEvent&& event);
        void ApplyZoneTransition(const ChatEvent& event);

        PacketWriter* writer = nullptr;
        INetworkSender* networkSender = nullptr;
        CorePerfCollector* perfCollector = nullptr;
        friend class Initializer;

    public:
        uint64_t AddChatSession(uint64_t session, uint16_t zone, uint32_t profileId) {
            uint64_t chatID = m_chatIdGenerater.fetch_add(1, std::memory_order_relaxed);

            {
                std::lock_guard<std::mutex> lock(m_zoneSequenceMutex);
                m_currentZoneSequence.insert_or_assign(session, 1);
            }

            ChatEvent e{};
            e.type = ChatEventType::SESSION_ADD;
            e.senderSessionID = session;
            e.key.id = zone;
            e.senderProfileId = profileId;
            e.senderChatID = chatID;
            e.zoneSequence = 1;
            EnqueueChat(e);
            return chatID;
        }

        void DeleteChatSession(uint64_t session, uint16_t zone) {
            ChatEvent e{};
            e.type = ChatEventType::SESSION_DELETE;
            e.senderSessionID = session;
            e.key.id = zone;
            EnqueueChat(e);
        }

        bool EnqueueChat(ChatEvent& event, Base::Priority priority = Base::Priority::Droppable) {
            if (!m_running.load(std::memory_order_relaxed))
                return false;

            return m_chatQueue.Enqueue(event, priority);
        }

        void EnqueueZoneTransition(ChatEvent& event) {
            const uint64_t session = event.senderSessionID;
            const uint64_t zoneID = event.key.id;

            std::lock_guard<std::mutex> lock(m_zoneSequenceMutex);
            auto it = m_currentZoneSequence.find(session);
            if (it == m_currentZoneSequence.end()) {
                errorLogger->LogError("chat thread", "zone transition before session add", "session", session, "zoneID", zoneID);
                return;
            }

            event.zoneSequence = it->second + 1;

            if (EnqueueChat(event, Base::Priority::Important)) {
                it->second = event.zoneSequence;
                return;
            }

            errorLogger->LogError("chat thread", "zone transition queue overflow","session", session, "zoneID", zoneID);
        }

        // EnqueueChat으로 같이 처리하면 코드가 복잡해짐
        void EnqueueZoneJoin(uint64_t session, uint16_t dest) {
            ChatEvent e{};
            e.type = ChatEventType::ZONE_JOIN;
            e.senderSessionID = session;
            e.key.id = dest;
            EnqueueZoneTransition(e);
        }

        void EnqueueZoneLeave(uint64_t session, uint16_t zoneID) {
            ChatEvent e{};
            e.type = ChatEventType::ZONE_LEAVE;
            e.senderSessionID = session;
            e.key.id = zoneID;
            EnqueueZoneTransition(e);
        }
	};
}
