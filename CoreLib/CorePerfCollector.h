#pragma once
#include <cstdint>
#include <atomic>
#include <array>
#include <thread>

#include "LoggerGlobal.h"
#include "Config.h"
namespace Core {
    struct ZonePerfData {// per sec
        std::atomic<uint64_t> packetProcessed;
        std::atomic<uint64_t> tick;
        std::atomic<uint64_t> actionFieldCount;
        std::atomic<uint64_t> deltaFieldCount; 
        std::atomic<uint64_t> fullFieldCount;
        std::atomic<uint64_t> monsterDeltaFieldCount;
        std::atomic<uint64_t> monsterFullFieldCount; 
        std::atomic<uint64_t> hitCount; 
    };

    class CorePerfCollector {
        std::array<ZonePerfData,ZONE_COUNT> zones;
        std::atomic<uint64_t> zoneWorkEnqueue;
        std::atomic<uint64_t> chatTick;
        std::atomic<uint64_t> chatSendCnt;
        std::atomic<uint64_t> broadcastEnqueuCnt;
        std::atomic<uint64_t> broadcastPopCnt;
        std::atomic<uint64_t> broadcastSendCnt;;
        std::thread m_thread;
        std::atomic<bool> m_running;

        void Initialize() {
            zoneWorkEnqueue.store(0, std::memory_order_relaxed);
            chatTick.store(0, std::memory_order_relaxed);
            chatSendCnt.store(0, std::memory_order_relaxed);
            broadcastEnqueuCnt.store(0, std::memory_order_relaxed);
            broadcastPopCnt.store(0, std::memory_order_relaxed);
            for (int i = 0; i < ZONE_COUNT; i++)
            {
                zones[i].packetProcessed.store(0, std::memory_order_relaxed);
                zones[i].tick.store(0, std::memory_order_relaxed);
                zones[i].actionFieldCount.store(0, std::memory_order_relaxed);
                zones[i].deltaFieldCount.store(0, std::memory_order_relaxed);
                zones[i].fullFieldCount.store(0, std::memory_order_relaxed);
                zones[i].monsterDeltaFieldCount.store(0, std::memory_order_relaxed);
                zones[i].monsterFullFieldCount.store(0, std::memory_order_relaxed);
                zones[i].hitCount.store(0, std::memory_order_relaxed);
            }
        }
        bool IsReady() {
            return true;
        }
        friend class Initializer;
    public:
        void Start() {
            m_thread = std::thread(&CorePerfCollector::ThreadFunc, this);
        }
        void Stop() {
            m_running.store(false, std::memory_order_relaxed);
            if (m_thread.joinable())
                m_thread.join();
            sysLogger->LogInfo("core perf", "Core perf collector thread stopped");
        }
        void ThreadFunc() {
            m_running.store(true, std::memory_order_relaxed);
            auto tid = std::this_thread::get_id();
            std::stringstream ss;
            ss << tid;
            sysLogger->LogInfo("core perf", "Core perf collector thread started", "threadID", ss.str());
            while (m_running.load(std::memory_order_relaxed))
            {
                Flush();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        void AddZoneEnqueueCnt() {
            zoneWorkEnqueue.fetch_add(1, std::memory_order_release);
        }
        void AddPacketProcessCnt(uint16_t zoneID, size_t cnt) {
            if (zoneID-1 >= ZONE_COUNT) {
                errorLogger->LogInfo("core perf", "AddPacketProcessCnt zoneID out of bound", "zoneID", zoneID);
                return;
            }
            zones[zoneID-1].packetProcessed.fetch_add(cnt, std::memory_order_relaxed);
            zones[zoneID-1].tick.fetch_add(1, std::memory_order_relaxed);

        }
        void AddActionFieldCnt(uint16_t zoneID, size_t cnt) {
            if (zoneID-1 >= ZONE_COUNT) {
                errorLogger->LogInfo("core perf", "AddActionPacketCnt zoneID out of bound", "zoneID", zoneID);
                return;
            }
            zones[zoneID-1].actionFieldCount.fetch_add(cnt, std::memory_order_relaxed);
        }
        void AddDeltaFieldCnt(uint16_t zoneID, size_t cnt) {
            if (zoneID-1 >= ZONE_COUNT) {
                errorLogger->LogInfo("core perf", "AddDeltaPacketCnt zoneID out of bound", "zoneID", zoneID);
                return;
            }
            zones[zoneID - 1].deltaFieldCount.fetch_add(cnt, std::memory_order_relaxed);
        }
        void AddFullFieldCnt(uint16_t zoneID, size_t cnt) {
            if (zoneID-1 >= ZONE_COUNT) {
                errorLogger->LogInfo("core perf", "AddFullPacketCnt zoneID out of bound", "zoneID", zoneID);
                return;
            }
            zones[zoneID - 1].fullFieldCount.fetch_add(cnt, std::memory_order_relaxed);
        }
        void AddMonsterDeltaFieldCnt(uint16_t zoneID, size_t cnt) {
            if (zoneID-1 >= ZONE_COUNT) {
                errorLogger->LogInfo("core perf", "AddMonsterDeltaPacketCnt zoneID out of bound", "zoneID", zoneID);
                return;
            }
            zones[zoneID - 1].monsterDeltaFieldCount.fetch_add(cnt, std::memory_order_relaxed);
        }
        void AddMonsterFullFieldCnt(uint16_t zoneID, size_t cnt) {
            if (zoneID-1 >= ZONE_COUNT) {
                errorLogger->LogInfo("core perf", "AddMonsterFullPacketCnt zoneID out of bound", "zoneID", zoneID);
                return;
            }
            zones[zoneID - 1].monsterFullFieldCount.fetch_add(cnt, std::memory_order_relaxed);
        }
        void AddHitCnt(uint16_t zoneID) {
            if (zoneID-1 >= ZONE_COUNT) {
                errorLogger->LogInfo("core perf", "AddHitCnt zoneID out of bound", "zoneID", zoneID);
                return;
            }
            zones[zoneID - 1].hitCount.fetch_add(1, std::memory_order_relaxed);
        }
        void ChatTick() {
            chatTick.fetch_add(1, std::memory_order_relaxed);
        }
        void AddChatSend(size_t cnt) {
            chatSendCnt.fetch_add(cnt, std::memory_order_relaxed);
        }
        void AddBroadcastEnqueueCnt() {
            broadcastEnqueuCnt.fetch_add(1, std::memory_order_relaxed);
        }
        void AddBroadcastPopCnt() {
            broadcastPopCnt.fetch_add(1, std::memory_order_relaxed);
        }
        void AddBroadcastSendCnt(size_t cnt) {
            broadcastSendCnt.fetch_add(cnt, std::memory_order_relaxed);
        }
        void Flush() {
            auto TzoneWorkEnqueue = zoneWorkEnqueue.load(std::memory_order_relaxed);
            zoneWorkEnqueue.store(0, std::memory_order_relaxed);
            auto TchatTick = chatTick.load(std::memory_order_relaxed);
            chatTick.store(0, std::memory_order_relaxed);
            auto TchatSendCnt = chatSendCnt.load(std::memory_order_relaxed);
            chatSendCnt.store(0, std::memory_order_relaxed);
            auto TbroadcastEnqueuCnt = broadcastEnqueuCnt.load(std::memory_order_relaxed);
            broadcastEnqueuCnt.store(0, std::memory_order_relaxed);
            auto TbroadcastPopCnt = broadcastPopCnt.load(std::memory_order_relaxed);
            broadcastPopCnt.store(0, std::memory_order_relaxed);
            auto TbroadcastSendCnt = broadcastSendCnt.load(std::memory_order_relaxed);
            broadcastSendCnt.store(0, std::memory_order_relaxed);
            perfLogger->LogInfo("core perf", "perf log per sec", "zoneWorkEnqueue", TzoneWorkEnqueue, 
            "chatTick", TchatTick, "broadcastEnqueuCnt", TbroadcastEnqueuCnt, "broadcastPopCnt", TbroadcastPopCnt,
            "broadcastSendCnt", TbroadcastSendCnt);
            for (int i = 0; i < ZONE_COUNT; i++)
            {
                auto& curr = zones[i];
                perfLogger->LogInfo("core perf", "zone thread per sec", "zoneID", i+1, 
                    "packetProcessed", curr.packetProcessed.load(std::memory_order_relaxed),
                    "tick", curr.tick.load(std::memory_order_relaxed),
                    "actionFieldCount", curr.actionFieldCount.load(std::memory_order_relaxed),
                    "deltaFieldCount", curr.deltaFieldCount.load(std::memory_order_relaxed),
                    "fullFieldCount", curr.fullFieldCount.load(std::memory_order_relaxed),
                    "monsterDeltaFieldCount", curr.monsterDeltaFieldCount.load(std::memory_order_relaxed),
                    "monsterFullFieldCount", curr.monsterFullFieldCount.load(std::memory_order_relaxed));
                curr.packetProcessed.store(0, std::memory_order_relaxed);
                curr.tick.store(0, std::memory_order_relaxed);
                curr.actionFieldCount.store(0, std::memory_order_relaxed);
                curr.deltaFieldCount.store(0, std::memory_order_relaxed);
                curr.fullFieldCount.store(0, std::memory_order_relaxed);
                curr.monsterDeltaFieldCount.store(0, std::memory_order_relaxed);
                curr.monsterFullFieldCount.store(0, std::memory_order_relaxed);
            }
        }
    };
}