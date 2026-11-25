#include "pch.h"
#include "NetPacketFilter.h"
#include "AccumulatedBuffer.h"

#include <CoreLib/Interface/IPacketView.h>
#include <CoreLib/Interface/IPacketDispatcher.h>
#include "NetPerfCollector.h"

namespace Net {
    PacketHeader NetPacketFilter::ParseHeader(AccumulatedBuffer& accBuf) {
        PacketHeader header;
        const size_t firstPart = (std::min)(sizeof(header), static_cast<size_t>(RING_BUFFER_SIZE - accBuf.GetFront()));
        std::memcpy(&header, accBuf.GetFrontPtr(), firstPart);
        if (firstPart < sizeof(header))
            std::memcpy(reinterpret_cast<uint8_t*>(&header) + firstPart, accBuf.GetStartPtr(), sizeof(header) - firstPart);
        return header; // wire의 uint32_t length를 검증 전에 축소하지 않는다.
    }

    bool NetPacketFilter::CheckPacketValid(AccumulatedBuffer& accBuf, const PacketHeader& header, uint64_t sessionID) {
        const auto magic = header.magic;
        const uint32_t packetLen = header.length;
        const auto opcode = header.opcode;

        uint16_t front = accBuf.GetFront();
        uint16_t rear = accBuf.GetRear();
        uint16_t len = accBuf.GetLen();
        if (magic != MAGIC) {
            Core::gameLogger->LogWarn("context", "packet Magic invalid", {{"session", sessionID}, {"magic", magic}, {"packetLen", packetLen}, {"opcode", opcode}, {"m_front", front}, {"m_rear", rear}, {"GetLen()", len}});
            return false;
        }

        if (opcode == 0 || opcode > MAX_DEFINED_OPCODE) {
            Core::gameLogger->LogWarn("context", "undefined opcode", {{"session", sessionID}, {"magic", magic}, {"packetLen", packetLen}, {"opcode", opcode}, {"m_front", front}, {"GetLen()", len}});
            return false;
        }

        if (packetLen < sizeof(PacketHeader) || packetLen > MAX_PACKET_LEN) {
            Core::gameLogger->LogWarn("context", "invalid packet length", {{"session", sessionID}, {"packetLen", packetLen}, {"opcode", opcode}});
            return false;
        }
        const int32_t expectedLen = GetReqPacketLen(opcode);
        if (expectedLen < 0)
            return false;
        const auto requiredLen = static_cast<uint32_t>(expectedLen);
        if (IsElasticReqPacket(opcode) ? packetLen < requiredLen : packetLen != requiredLen)
            return false;

        if (IsSimulationFlag(header.flags) != IsSimulationOpcode(opcode))
            return false;
        return true;
    }

    PacketValidation NetPacketFilter::HasCompletePacket(AccumulatedBuffer& accBuf,  PacketHeader& header, uint64_t sessionID) {
        if (accBuf.GetLen() < sizeof(PacketHeader))
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
        if (op == PONG_OPCODE) {
            Pong body;
            std::memcpy(&body, pv->GetPtr() + PACKET_HEADER_SIZE, sizeof(body));
            uint64_t now = clock->GetTimeMS();
            uint64_t rtt = body.serverTimeMs <= now ? now - body.serverTimeMs : 0;
            if (rtt > jitter.RTO(rtt))
                perfCollector->AddJitterCnt();
            pongRTT = rtt;
            return true;
        }

        if (!(health & MASK_EXIST)) {
            if (op == AUTH_OPCODE) {
                packetDispatcher->Process(std::move(pv));
                return true;
            }
            Core::gameLogger->LogWarn("net filter", "session not exist", {{"session", session}});
            return false;
        }

        if (!(health & MASK_NOT_CHEAT)) {
            Core::gameLogger->LogWarn("net filter", "cheat detect", {{"session", session}});
            return false;
        }
        if (!(health & MASK_AUTHENTICATED)) {
            Core::gameLogger->LogWarn("net filter", "session not authenticated", {{"session", session}});
            switch (op) {
            case AUTH_OPCODE: {
                packetDispatcher->Process(std::move(pv));
                // 인증 실패를 바로 Disconnect 시키지 않기 때문에 재인증 허용
                return true;
            }
            case PONG_OPCODE: break;
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
