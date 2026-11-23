#include "pch.h"
#include "ChatThreadPool.h"
#include "../Interface/INetworkSender.h"
#include "../Packet/PacketWriter.h"
#include "../CorePerfCollector.h"

namespace Core {
    uint64_t ChatThreadPool::OnSessionEnteredWorld(uint64_t sessionID, uint16_t zoneID, uint32_t profileID) {
        const auto chatID = AddChatSession(sessionID, profileID);
        EnqueueZoneJoin(sessionID, zoneID);
        return chatID;
    }

    void ChatThreadPool::OnSessionZoneChanged(uint64_t sessionID, uint16_t previousZoneID, uint16_t currentZoneID) {
        if (currentZoneID != 0)
            EnqueueZoneJoin(sessionID, currentZoneID);
        else if (previousZoneID != 0)
            EnqueueZoneLeave(sessionID, previousZoneID);
    }

    void ChatThreadPool::OnSessionClosed(uint64_t sessionID, uint16_t zoneID) {
        if (zoneID != 0)
            EnqueueZoneLeave(sessionID, zoneID);
        DeleteChatSession(sessionID);
    }

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

        while (m_running.load(std::memory_order_relaxed)) {
            int processed = 0;
            for (; processed < CHAT_BATCH_SIZE; ++processed) {
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

        std::visit(EventVisitor{*this, tempPackets}, curr);
        return true;
    }

    void ChatThreadPool::EventVisitor::operator()(const ChatSessionAddEvent& event) const {
        pool.m_sessions.AddSession(event.senderSessionID, event.senderChatID, event.senderProfileId);
    }

    void ChatThreadPool::EventVisitor::operator()(const ChatZoneJoinEvent& event) const {
        pool.ProcessZoneTransition(event);
    }

    void ChatThreadPool::EventVisitor::operator()(const ChatZoneLeaveEvent& event) const {
        pool.ProcessZoneTransition(event);
    }

    void ChatThreadPool::EventVisitor::operator()(const ChatMessageEvent& event) const {
        const auto* user = pool.m_sessions.FindSession(event.senderSessionID);
        if (user)
            pool.ProcesChat(event, user->chatID, user->profileId, tempPackets);
    }

    void ChatThreadPool::EventVisitor::operator()(const ChatSessionDeleteEvent& event) const {
        if (pool.m_sessions.FindSession(event.senderSessionID))
            pool.m_sessions.DeleteSession(event.senderSessionID);
    }

    void ChatThreadPool::FlushChatBatches(std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
        for (auto& [key, packet] : tempPackets) {
            SendPacketGroup(key, packet);
        }

        tempPackets.clear();
    }

    void ChatThreadPool::SendWhisper(const ChatMessageEvent& event, uint64_t chatID, uint32_t profileId) {
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
        switch (key.scope) {
        case CHAT_SCOPE::Zone: {
            const auto* members = m_sessions.FindZoneMembers(key.id);
            if (!members)
                return;
            for (auto session : *members) {
                networkSender->SendData(session, packet);
            }
            recipientCount = members->size();
            break;
        }
        case CHAT_SCOPE::Global: {
            const auto& sessions = m_sessions.GetSessions();
            for (const auto& [session, user] : sessions) {
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

    void ChatThreadPool::ProcesChat(const ChatMessageEvent& curr, uint64_t chatID, uint32_t profileId,
        std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
        if (curr.key.scope == CHAT_SCOPE::Whisper) {
            SendWhisper(curr, chatID, profileId);
            return;
        }
        if (curr.key.scope == CHAT_SCOPE::Zone && !m_sessions.FindZoneMembers(curr.key.id))
            return;

        AppendToChatBatch(curr, chatID, profileId, tempPackets);
    }

    void ChatThreadPool::AppendToChatBatch(const ChatMessageEvent& curr, uint64_t chatID, uint32_t profileId,
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

    template<typename ZoneEvent>
    void ChatThreadPool::ProcessZoneTransition(const ZoneEvent& event) {
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

    void ChatThreadPool::ApplyZoneTransition(const ChatZoneJoinEvent& event) {
        m_sessions.JoinZone(event.senderSessionID, event.zoneID);
    }

    void ChatThreadPool::ApplyZoneTransition(const ChatZoneLeaveEvent& event) {
        m_sessions.LeaveZone(event.senderSessionID);
    }

}
