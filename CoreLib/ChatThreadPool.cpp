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
                if (!ProcessQueuedEvent(tempPackets))
                    break;
            }

            FlushChatBatches(tempPackets);

            // Batch가 가득 찬 동안은 sleep 없이 다음 batch를 즉시 처리한다.
            if (processed == CHAT_BATCH_SIZE)
                continue;

            std::this_thread::sleep_for(CHAT_BATCH_WINDOW);
        }
    }

    bool ChatThreadPool::ProcessQueuedEvent(std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
        if (!m_running.load(std::memory_order_relaxed))
            return false;

        ChatEvent curr;
        if (!m_chatQueue.Dequeue(curr))
            return false;

        if (!m_running.load(std::memory_order_relaxed))
            return false;

        switch (curr.type)
        {
        case ChatEventType::SESSION_ADD:
            m_sessions.AddSession(curr.senderSessionID, curr.senderChatID, curr.senderProfileId);
            return true;
        case ChatEventType::ZONE_JOIN:
        case ChatEventType::ZONE_LEAVE:
            ProcessZoneTransition(std::move(curr));
            return true;
        }

        const auto* user = m_sessions.FindSession(curr.senderSessionID);
        if (!user) {
            return true;
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
        default:
            errorLogger->LogInfo("chat thread", "undefined chat event", "chatID", chatID, "chat event type", (int)curr.type);
            break;
        }
        return true;
    }

    void ChatThreadPool::FlushChatBatches(std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
        for (auto& [key, packet] : tempPackets)
        {
            SendPacketGroup(key, packet);
        }

        tempPackets.clear();
    }

    void ChatThreadPool::SendWhisper(const ChatEvent& event, uint64_t chatID, uint32_t profileId) {
        auto packet = writer->GetChatWhisperPacket(chatID, profileId, event.message);
        if (!packet)
            return;

        const auto destSession = m_sessions.FindSessionID(event.key.id);
        if (!destSession) {
            return;
        }

        networkSender->SendDataUnique(*destSession, std::move(packet));
        auto senderPacket = writer->GetChatWhisperPacket(chatID, profileId, event.message);
        if (senderPacket) {
            networkSender->SendDataUnique(event.senderSessionID, std::move(senderPacket));
            perfCollector->AddChatSend(2);
        } else {
            perfCollector->AddChatSend(1);
        }
    }

    void ChatThreadPool::SendPacketGroup(ChatDestKey key, std::shared_ptr<IPacket> packet) {
        size_t recipientCount;
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
            recipientCount = members->size();
            break;
        }
        case CHAT_SCOPE::Global: {
            const auto& sessions = m_sessions.GetSessions();
            for (const auto& [session, user] : sessions)
            {
                networkSender->SendData(session, packet);
            }
            recipientCount = sessions.size();
            break;
        }
        default:
            return;
        }

        const auto* batch = parseBody<ChatBatchNotifyBody>(packet->GetBuffer());
        perfCollector->AddChatSend(batch->chatCnt * recipientCount);
    }

    void ChatThreadPool::ProcesChat(ChatEvent& curr, uint64_t chatID, uint32_t profileId, std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
        if (curr.key.scope == CHAT_SCOPE::Whisper) {
            SendWhisper(curr, chatID, profileId);
            return;
        }
        if (curr.key.scope == CHAT_SCOPE::Zone && !m_sessions.FindZoneMembers(curr.key.id))
            return;

        AppendToChatBatch(curr, chatID, profileId, tempPackets);
    }

    void ChatThreadPool::AppendToChatBatch(const ChatEvent& curr, uint64_t chatID, uint32_t profileId,
        std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
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
            errorLogger->LogError("chat thread", "zone transition before session add", "session", event.senderSessionID, "sequence", event.zoneSequence);
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
