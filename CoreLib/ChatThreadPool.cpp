#include "pch.h"
#include "ChatThreadPool.h"
#include "IIOCP.h"
#include "PacketWriter.h"

namespace Core {
    void ChatThreadPool::ThreadFunc() {
        m_running.store(true);
        std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash> tempPackets;
        tempPackets.reserve(100);

        while (m_running)
        {
            int loop = 100; 
            bool processed = false;
            ChatEvent curr;

            while (loop-- and m_chatQueue.pop(curr)) {
                processed = true;
                switch (curr.type)
                {
                case ChatEventType::SESSION_ADD:
                    ProcessAddSession(curr.senderSessionID, curr.senderChatID, static_cast<uint16_t>(curr.key.id), curr.message);
                    break;
                }

                auto it = m_sessionChatIdMap.find(curr.senderSessionID);
                if (it == m_sessionChatIdMap.end()) {
                        continue;
                }
                auto chatID = it->second.chatID;
                auto userName = it->second.userName;
                
                switch (curr.type)
                {
                case ChatEventType::CHAT:
                    ProcesChat(curr, chatID, userName, tempPackets);
                    break;

                case ChatEventType::SESSION_DELETE:
                    ProcessDeleteSession(curr.senderSessionID, static_cast<uint16_t>(curr.key.id));
                    break;
                case ChatEventType::ZONE_JOIN:
                    ProcessZoneJoin(curr.senderSessionID, static_cast<uint16_t>(curr.key.id));
                    break;
                case ChatEventType::ZONE_LEAVE:
                    ProcessZoneLeave(curr.senderSessionID, static_cast<uint16_t>(curr.key.id));
                    break;
                    //case ChatEventType::GUILD_JOIN: break;
                    //case ChatEventType::GUILD_LEAVE: break;
                    //case ChatEventType::PARTY_JOIN: break;
                    //case ChatEventType::PARTY_LEAVE: break;
                default:
                    break;
                   // "undefined chat event type"
                }
            }
            for (auto& [key, packet] : tempPackets)
            {
                SendPacketGroup(key, packet);
            }

            tempPackets.clear();

            if (!processed) {
                // busy spin 방지용 sleep
                // batch 처리량과 응답latency 간 trade-off 조절 (TPS 측정 기준)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
    void ChatThreadPool::SendPacket(uint64_t session, std::shared_ptr<IPacket> p) {
        iocp->SendData(session, p);
    }
    void ChatThreadPool::SendPacketGroup(ChatDestKey key, std::shared_ptr<IPacket> packet) {
        switch (key.scope)
        {
        case CHAT_SCOPE::Zone: {
            uint16_t zoneID = key.id;
            if (zoneID == 0 || zoneID > ZONE_COUNT + 1)
                return;
            for (auto& session : m_zoneMembers[zoneID-1])
            {
                iocp->SendData(session, packet);
            }
            break;
        }
        case CHAT_SCOPE::Global: {
            for (auto& [session, chatID] : m_sessionChatIdMap)
            {
                iocp->SendData(session, packet);
            }
            break;
        }
        default: break;
        }
    }

    void ChatThreadPool::ProcesChat(ChatEvent& curr, uint64_t chatID, std::string& userName, std::unordered_map<ChatDestKey, std::shared_ptr<IPacket>, ChatDestKeyHash>& tempPackets) {
        if (curr.key.scope == CHAT_SCOPE::Whisper) {
            auto packet = writer->GetChatWhisperPacket(chatID, userName, curr.message);
            uint64_t& destChatID = curr.key.id;
            auto it = m_chatIdSessionMap.find(destChatID);
            if (it == m_chatIdSessionMap.end()) {
                // whisper dest user not exist
                return;
            }
            SendPacket(it->second, packet);
            SendPacket(curr.senderSessionID, packet);
        }

        auto it = tempPackets.find(curr.key);
        if (it != tempPackets.end()) {
            uint16_t count = writer->WriteChatBatchPacketField(it->second, chatID, userName, curr.message);
            if (count == MAX_CHAT_PACKET) {
                SendPacketGroup(it->first, it->second);
                tempPackets[curr.key] = writer->GetInitialChatBatchPacket(curr.key.scope);
            }
        }
        else {
            tempPackets[curr.key] = writer->GetInitialChatBatchPacket(curr.key.scope);
            int count = writer->WriteChatBatchPacketField(tempPackets[curr.key], chatID, userName, curr.message);
        }
    }

    // State Manager의 처리결과를 동기화해서 쓰는거라서 예외 발생은 하지 않는다고 가정.
    // session 추가, 삭제는 샤딩 적용하더라도, mutex 필요.
    void ChatThreadPool::ProcessAddSession(uint64_t sessionID, uint64_t chatID, uint16_t zoneID, std::string& userName) {
        auto it = m_sessionChatIdMap.find(sessionID);
        if (it != m_sessionChatIdMap.end()) {
            // "Chat ID exist in map";
            return;
        }
        auto& node = m_sessionChatIdMap[sessionID];
        node.chatID = chatID; 
        node.userName = userName;
        m_chatIdSessionMap[chatID] = sessionID;
        // 싱글 스레드 접근이라서 안전함
    }
    void ChatThreadPool::ProcessDeleteSession(uint64_t sessionID, uint16_t zoneID) {
        auto it = m_sessionChatIdMap.find(sessionID);
        if (it == m_sessionChatIdMap.end()) {
           // "Chat ID doesn't exist in map";
            return;
        }
        m_chatIdSessionMap.erase(it->second.chatID);
        m_sessionChatIdMap.erase(it);
    }
    void ChatThreadPool::ProcessZoneJoin(uint64_t sessionID, uint16_t zoneID) {
        auto it = m_zoneMembers[zoneID-1].find(sessionID);
        if (it != m_zoneMembers[zoneID-1].end()) {
            // "session already exist in zone"
            return;
        }
        m_zoneMembers[zoneID-1].insert(sessionID);
    }
    void ChatThreadPool::ProcessZoneLeave(uint64_t sessionID, uint16_t zoneID) {
        auto it = m_zoneMembers[zoneID-1].find(sessionID);
        if (it == m_zoneMembers[zoneID-1].end()) {
            // "session dosen't exist in zone"
            return;
        }
        m_zoneMembers[zoneID-1].erase(sessionID);
    }

}