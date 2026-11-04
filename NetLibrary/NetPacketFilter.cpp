#include "pch.h"
#include "NetPacketFilter.h"
#include "AccumulatedBuffer.h"

#include <CoreLib/IPacketView.h>
#include <CoreLib/IPacketDispatcher.h>
#include <CoreLib/Config.h>
#include "NetPerfCollector.h"

namespace Net {
    Core::PacketHeader NetPacketFilter::ParseHeader(AccumulatedBuffer& accBuf) {
        Core::PacketHeader header;
        const size_t firstPart = (std::min)(sizeof(header), static_cast<size_t>(RING_BUFFER_SIZE - accBuf.GetFront()));
        std::memcpy(&header, accBuf.GetFrontPtr(), firstPart);
        if (firstPart < sizeof(header))
            std::memcpy(reinterpret_cast<uint8_t*>(&header) + firstPart, accBuf.GetStartPtr(), sizeof(header) - firstPart);
        return header; // wire의 uint32_t length를 검증 전에 축소하지 않는다.
    }

    bool NetPacketFilter::CheckPacketValid(AccumulatedBuffer& accBuf, const Core::PacketHeader& header, uint64_t sessionID) {
        const auto magic = header.magic;
        const uint32_t packetLen = header.length;
        const auto opcode = header.opcode;

        uint16_t front = accBuf.GetFront();
        uint16_t rear = accBuf.GetRear();
        uint16_t len = accBuf.GetLen();
        if (magic != Core::MAGIC) {
            Core::gameLogger->LogWarn("context", "packet Magic invalid", "session", sessionID, "magic", magic, "packetLen", packetLen, "opcode", opcode, "m_front", front, "m_rear", rear, "GetLen()", len);
            return false;
        }

        if (opcode == 0 || opcode > Core::MAX_DEFINED_OPCODE) {
            Core::gameLogger->LogWarn("context", "undefined opcode", "session", sessionID, "magic", magic, "packetLen", packetLen, "opcode", opcode, "m_front", front, "GetLen()", len);
            return false;
        }

        if (packetLen < sizeof(Core::PacketHeader) || packetLen > MAX_PACKET_LEN) {
            Core::gameLogger->LogWarn("context", "invalid packet length", "session", sessionID, "packetLen", packetLen, "opcode", opcode);
            return false;
        }
        // 고정 길이 요청이 짧은 본문이나 잘못된 dispatcher로 전달되지 않도록 한다.
        if (opcode == Core::OP::BAZAAR_SEARCH
            && (header.flags != 0 || packetLen != sizeof(Core::PacketStruct<Core::BazaarSearchRequestBody>)))
            return false;
        if (opcode == Core::OP::BAZAAR_MY_LIST
            && (header.flags != 0 || packetLen != sizeof(Core::PacketStruct<Core::BazaarMyListRequestBody>)))
            return false;
        if (opcode == Core::OP::ITEM_USE
            && (header.flags != 0 || packetLen != sizeof(Core::PacketStruct<Core::ItemUseRequestBody>)))
            return false;
        return true;
    }

    PacketValidation NetPacketFilter::HasCompletePacket(AccumulatedBuffer& accBuf,  Core::PacketHeader& header, uint64_t sessionID) {
        if (accBuf.GetLen() < sizeof(Core::PacketHeader))
            return PacketValidation::Incomplete;
        header = ParseHeader(accBuf);
        
        if (!CheckPacketValid(accBuf, header, sessionID)) {
            return PacketValidation::Invalid;
        }
        if (accBuf.GetLen() < header.length)
            return PacketValidation::Incomplete;
        return PacketValidation::Complete;
	}

    bool NetPacketFilter::TryDispatch(std::unique_ptr<Core::IPacketView, Core::PacketViewDeleter> pv, Jitter& jitter, std::optional<uint64_t>& pongRTT) {
        auto session = pv->GetSessionID();
        auto op = pv->GetOpcode();
        // 게임 세션 상태 확인. 
        uint8_t health = packetDispatcher->HealthCheck(session);
        if (op == Core::OP::PONG) {
            uint64_t rtt = packetDispatcher->GetRTT(std::move(pv), clock->GetTimeMS());
            if (rtt > jitter.RTO(rtt))
                perfCollector->AddJitterCnt();
            pongRTT = rtt;
            return true;
        }

        if (!(health & Core::MASK_EXIST)) {
            if (op == Core::OP::AUTH) {
                packetDispatcher->Process(std::move(pv));
                return true;
            }
            Core::gameLogger->LogWarn("net filter", "session not exist", "session", session);
            return false;
        }

        if (!(health & Core::MASK_NOT_CHEAT)) {
            Core::gameLogger->LogWarn("net filter", "cheat detect", "session", session);
            return false;
        }
        if (!(health & Core::MASK_AUTHENTICATED)) {
            Core::gameLogger->LogWarn("net filter", "session not authenticated", "session", session);
            switch (op) {
            case ::Core::OP::AUTH: {
                packetDispatcher->Process(std::move(pv));
                // 인증 실패를 바로 Disconnect 시키지 않기 때문에 재인증 허용
                return true;
            }
            case ::Core::OP::PONG: break;
            default: return false;
            }
        }

        switch (op)
        {
        default:
            packetDispatcher->Process(std::move(pv));
            break;
        }

        return true;
        
    }

}
