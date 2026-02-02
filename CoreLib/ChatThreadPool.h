#pragma once
#include <mutex>
#include <thread>
#include <atomic>
#include <set>
#include <unordered_map>
#include <memory>

#include "PacketTypes.h"
#include "Config.h"
#include <CoreLib/IPacket.h>
#include <BaseLib/LockFreeQueue.h>

namespace Core {
    enum class ChatEventType {
        SESSION_ADD,
        SESSION_DELETE,
        CHAT,
        ZONE_JOIN,
        ZONE_LEAVE,
        //PARTY_JOIN,
        //PARTY_LEAVE,
        //GUILD_JOIN,
        //GUILD_LEAVE,
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
        ChatDestKey key;
        std::string message;
    };

    struct ChatUserInfo {
        uint64_t chatID;
        std::string userName;
    };
    
    class IIOCP;
    class PacketWriter;
    class ChatThreadPool {
		// Chat 패킷 처리. 확장 및 성능을 위해 Zone Thread와 분리.
        // Chat 확장성을 위해 사용. (전채 채널 채팅, 귓속말, 채팅창을 통한 상호작용 등)
        
        std::unordered_map<uint64_t, ChatUserInfo> m_sessionChatIdMap;
        std::unordered_map<uint64_t, uint64_t> m_chatIdSessionMap;
        // 역방향 매핑, Whisper 시에 ChatID를 통해 전송하기 위해 필요함. 
        // Session을 클라이언트에 알려주는 것은 위험.

        std::vector<std::set<uint64_t>> m_zoneMembers; 
        // set에는 session 담기 -> broadcast에 사용
        // zone 당 2000명으로 설정해서, 삽입 삭제 비용이 크지 않음.
        //std::unordered_map<uint64_t, std::vector<uint64_t>> m_guildMembers;
        //std::unordered_map<uint64_t, std::vector<uint64_t>> m_partyMembers;
        std::atomic<uint64_t> m_chatIdGenerater;
        Base::LockFreeQueue<ChatEvent, CHAT_QUEUE_SIZE> m_chatQueue;
        // MPMC 큐 적용

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

        void Initialize(IIOCP* i, PacketWriter* w) {
            iocp = i;
            writer = w;
            m_sessionChatIdMap.reserve(MAX_USER_CAPACITY);
            m_zoneMembers.resize(ZONE_COUNT);
            m_chatIdGenerater.store(1);
        }

        bool IsReady() {
            if (iocp == nullptr)
                return false;
            if (writer == nullptr)
                return false;
            return true;
        }
        void Start() {
            const uint16_t CHAT_WORKER_SIZE = 1;  // 변경하려면 ChatDestKey 활용해 샤딩 적용 필요. 
            for (int i = 0; i < CHAT_WORKER_SIZE; i++)
            {
                m_workerThreadPool.emplace_back(std::thread(&ChatThreadPool::ThreadFunc, this));
            }
        }

        void Stop() {
            m_running.store(false);
            for (auto& t : m_workerThreadPool)
            {
                if (t.joinable())
                    t.join();
            }
        }

        void ThreadFunc();
        void SendPacketUnique(uint64_t session, std::unique_ptr<IPacket, PacketDeleter> p);
        void SendPacketGroup(ChatDestKey key, std::shared_ptr<IPacket> packet);
        void ProcesChat(ChatEvent& curr, uint64_t chatID, std::string& userName,
            std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets);
        void ProcessAddSession(uint64_t sessionID, uint64_t chatID, uint16_t zoneID, std::string& userName);
        void ProcessDeleteSession(uint64_t sessionID, uint16_t zoneID);
        void ProcessZoneLeave(uint64_t sessionID, uint16_t zoneID);
        void ProcessZoneJoin(uint64_t sessionID, uint16_t zoneID);

        PacketWriter* writer;
        IIOCP* iocp;
        friend class Initializer;

    public:
        uint64_t AddChatSession(uint64_t session, uint16_t zone, std::string&& userName) {
            uint64_t chatID = m_chatIdGenerater.fetch_add(1);
            ChatEvent e{};
            e.type = ChatEventType::SESSION_ADD;
            e.senderSessionID = session;
            e.key.id = zone; 
            e.message = std::move(userName);
            e.senderChatID = chatID;
            // char array를 std::string으로 변환한 뒤
            // rvalue로 전달하여 이후 복사를 최소화한다. (길이 짧아서 성능 차이는 미미함.)
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

        void EnqueueChat(ChatEvent& event) {
            m_chatQueue.push(event);
        }

        // EnqueueChat으로 같이 처리하면 코드가 복잡해짐
        void EnqueueZoneJoin(uint64_t session, uint16_t dest) {
            ChatEvent e{};
            e.type = ChatEventType::ZONE_JOIN;
            e.senderSessionID = session;
            e.key.id = dest;
            EnqueueChat(e);
        }

        void EnqueueZoneLeave(uint64_t session, uint16_t zoneID) {
            ChatEvent e{};
            e.type = ChatEventType::ZONE_LEAVE;
            e.senderSessionID = session;
            e.key.id = zoneID;
            EnqueueChat(e);
        }
	};
}