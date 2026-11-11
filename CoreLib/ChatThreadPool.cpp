#include "pch.h"
#include "ChatThreadPool.h"
#include "INetworkSender.h"
#include "PacketWriter.h"
#include "CorePerfCollector.h"

namespace Core {
    namespace {
        constexpr auto CHAT_BATCH_WINDOW = std::chrono::milliseconds(15);
        constexpr int CHAT_BATCH_SIZE = 100;
    }

    void ChatThreadPool::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        sysLogger->LogInfo("chat thread", "chat thread started", "threadID", ss.str());
        std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash> tempPackets;
        tempPackets.reserve(100);

        while (m_running.load(std::memory_order_relaxed))
        {
            int processed = 0;
            for (; processed < CHAT_BATCH_SIZE; ++processed)
            {
                if (!m_running.load(std::memory_order_relaxed))
                    break;

                ChatEvent curr;
                if (!m_chatQueue.Dequeue(curr))
                    break;

                if (!m_running.load(std::memory_order_relaxed))
                    break;

                switch (curr.type)
                {
                case ChatEventType::SESSION_ADD:
                    m_sessions.AddSession(curr.senderSessionID, curr.senderChatID, curr.senderProfileId);
                    continue;
                case ChatEventType::ZONE_JOIN:
                case ChatEventType::ZONE_LEAVE:
                    ProcessZoneTransition(std::move(curr));
                    continue;
                }

                const auto* user = m_sessions.FindSession(curr.senderSessionID);
                if (!user) {
                    continue;
                }
                auto chatID = user->chatID;
                auto profileId = user->profileId;

                switch (curr.type)
                {
                case ChatEventType::CHAT:
                    ProcesChat(curr, chatID, profileId, tempPackets);
                    break;

                case ChatEventType::SESSION_DELETE:
                    m_sessions.DeleteSession(curr.senderSessionID);
                    break;
                    //case ChatEventType::GUILD_JOIN: break;
                    //case ChatEventType::GUILD_LEAVE: break;
                    //case ChatEventType::PARTY_JOIN: break;
                    //case ChatEventType::PARTY_LEAVE: break;
                default:
                    errorLogger->LogInfo("chat thread", "undefined chat event", "chatID", chatID, "chat event type", (int)curr.type);
                    break;
                    // "undefined chat event type"
                }
            }

            for (auto& [key, packet] : tempPackets)
            {
                SendPacketGroup(key, packet);
            }

            tempPackets.clear();

            // Batch가 가득 찬 동안은 sleep 없이 다음 batch를 즉시 처리한다.
            if (processed == CHAT_BATCH_SIZE)
                continue;

            std::this_thread::sleep_for(CHAT_BATCH_WINDOW);
        }
    }
    void ChatThreadPool::SendPacketUnique(uint64_t session, std::unique_ptr<IPacket, PacketDeleter> p) {
        networkSender->SendDataUnique(session, std::move(p));
    }
    void ChatThreadPool::SendPacketGroup(ChatDestKey key, std::shared_ptr<IPacket> packet) {
        switch (key.scope)
        {
        case CHAT_SCOPE::Zone: {
            const auto* members = m_sessions.FindZoneMembers(key.id);
            if (!members)
                return;
            for (auto session : *members)
            {
                networkSender->SendData(session, packet);
            }
            break;
        }
        case CHAT_SCOPE::Global: {
            for (const auto& [session, user] : m_sessions.GetSessions())
            {
                networkSender->SendData(session, packet);
            }
            break;
        }
        default: break;
        }
    }

    void ChatThreadPool::ProcesChat(ChatEvent& curr, uint64_t chatID, uint32_t profileId, std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
        if (curr.key.scope == CHAT_SCOPE::Whisper) {
            auto packet = writer->GetChatWhisperPacket(chatID, profileId, curr.message);
            if (!packet)
				return;
            const auto destSession = m_sessions.FindSessionID(curr.key.id);
            if (!destSession) {
                return;
            }
            SendPacketUnique(*destSession, std::move(packet));
            auto senderPacket = writer->GetChatWhisperPacket(chatID, profileId, curr.message);
            if (senderPacket) {
                SendPacketUnique(curr.senderSessionID, std::move(senderPacket));
                perfCollector->AddChatSend(2);
            } else {
                perfCollector->AddChatSend(1);
            }
            return;
        }
        if (curr.key.scope == CHAT_SCOPE::Global) {
            perfCollector->AddChatSend(m_sessions.GetSessions().size());
        }
        if (curr.key.scope == CHAT_SCOPE::Zone) {
            const auto* members = m_sessions.FindZoneMembers(curr.key.id);
            if (!members)
                return;
            perfCollector->AddChatSend(members->size());
        }
        auto it = tempPackets.find(curr.key);
        if (it != tempPackets.end()) {
            uint16_t count = writer->WriteChatBatchPacketField(it->second, chatID, profileId, curr.message);
            if (count == 0 || count == MAX_CHAT_PACKET) {
                SendPacketGroup(it->first, it->second);
                tempPackets.erase(it);
            }
            if (count != 0)
                return;
        }

        auto packet = writer->GetInitialChatBatchPacket(curr.key.scope);
        if (packet && writer->WriteChatBatchPacketField(packet, chatID, profileId, curr.message) != 0)
            tempPackets.emplace(curr.key, std::move(packet));
    }

    void ChatThreadPool::ProcessZoneTransition(ChatEvent&& event) {
        std::lock_guard<std::mutex> lock(m_zoneSequenceMutex);
        auto sequenceIt = m_currentZoneSequence.find(event.senderSessionID);
        if (sequenceIt == m_currentZoneSequence.end()) {
            errorLogger->LogError(
                "chat thread",
                "zone transition before session add",
                "session",
                event.senderSessionID,
                "sequence",
                event.zoneSequence);
            return;
        }

        if (event.zoneSequence != sequenceIt->second) {
            return;
        }

        ApplyZoneTransition(event);
    }

    void ChatThreadPool::ApplyZoneTransition(const ChatEvent& event) {
        if (event.type == ChatEventType::ZONE_JOIN) {
            m_sessions.JoinZone(event.senderSessionID, static_cast<uint16_t>(event.key.id));
        }
        else if (event.type == ChatEventType::ZONE_LEAVE) {
            m_sessions.LeaveZone(event.senderSessionID);
        }
    }

}
