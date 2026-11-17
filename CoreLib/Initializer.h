#pragma once

#include <array>
#include <memory>

#include "CacheCommandSender.h"
#include "InMemoryQueue.h"
#include "MessagePool.h"
#include "MessageQueueHandler.h"
#include "MessageWriter.h"
#include "ZoneState.h"
#include "PacketWriter.h"
#include "INetworkSender.h"
#include "IMessageQueue.h"
#include "StateManager.h"
#include "BroadcastThreadPool.h"
#include "ZoneEventRouter.h"
#include "ZoneHandler.h"
#include "NonZoneHandler.h"
#include "NonZoneThreadPool.h"
#include "PacketDispatcher.h"
#include "LobbyZone.h"
#include "IPingPacketWriter.h"
#include "ChatThreadPool.h"
#include "IProfileCache.h"
#include "DurableLogger.h"

#include "LoggerGlobal.h"

namespace Core {
    class Initializer {
        CoreMessagePool msgPool;
        InMemoryQueue recvMQ;
        MessageQueueHandler mqHandler;
        PacketWriter writer;
        MessageWriter messageWriter;
        CacheCommandSender cacheCommandSender;
        StateManager stateManager;
        BroadcastThreadPool broadcastPool;
        ZoneHandler zoneHandler;
        ZoneEventRouter zoneEventRouter;
        NonZoneHandler nonZoneHandler;
        NonZoneThreadPool nonZoneThreadPool;
        PacketDispatcher packetDispatcher;
        LobbyZone lobbyZone;
        ChatThreadPool chat;
        CorePerfCollector perfCollector;

    public:
        void Initialize() {
        }
        ~Initializer() {
            StopProducers();
            FinishShutdown();
        }
        
        void InjectDependencies1(INetworkSender* networkSender, IPacketPoolManager* packetPoolManager) {
            perfCollector.Initialize();
            broadcastPool.Initialize(networkSender, &stateManager, &perfCollector, &writer);
            lobbyZone.Initialize(& stateManager);
            nonZoneThreadPool.Initialize(&nonZoneHandler);
            zoneEventRouter.Initialize(zoneHandler, perfCollector);
            writer.Initialize(packetPoolManager);
            mqHandler.Initialize(networkSender, &writer, &lobbyZone, &stateManager);
            recvMQ.Initialize(&mqHandler, &msgPool);
            recvMQ.Start();
            chat.Initialize(networkSender, &writer, &perfCollector);
        }
        
        void InjectDependencies2(INetworkSender* networkSender, ISessionAuth* session, IMessageQueue* sendMQ, IProfileCache* profileCache, IInventoryOperations* inventory = nullptr) {
            stateManager.inventory = inventory;
            cacheCommandSender.Initialize(sendMQ, &messageWriter, &msgPool);
            stateManager.Initialize(&cacheCommandSender, networkSender, &lobbyZone, &chat);
            packetDispatcher.Initialize(&nonZoneThreadPool, &zoneEventRouter, &stateManager);
            nonZoneThreadPool.Start();
            broadcastPool.Start();
            ZoneState::Initialize(&broadcastPool, &writer, &stateManager, &perfCollector);
            zoneHandler.Initialize(&stateManager, networkSender, &writer);
            nonZoneHandler.Initialize(networkSender, session, &writer, &cacheCommandSender, &stateManager, &lobbyZone, &chat, profileCache);
            std::array<ZoneState*, ZONE_COUNT> zones{};
            for (size_t i = 0; i < zones.size(); ++i)
                zones[i] = stateManager.GetZone(static_cast<uint16_t>(i + 1));
            zoneEventRouter.Start(zones);
            chat.Start();
            perfCollector.Start();
        }
        
        bool CheckReady() {
            if (!writer.IsReady()
                or !msgPool.IsReady()
                or !mqHandler.IsReady()
                or !recvMQ.IsReady()
                or !zoneHandler.IsReady()
                or !ZoneState::IsReady()
                or !broadcastPool.IsReady()
                or !zoneEventRouter.IsReady()
                or !packetDispatcher.IsReady()
                or !lobbyZone.IsReady()
                or !cacheCommandSender.IsReady()
                or !stateManager.IsReady()
                or !chat.IsReady()
                or !perfCollector.IsReady()) {
                return false;
            }
            return true;
        }

        IPacketDispatcher* GetPacketDispatcher() {
            return static_cast<IPacketDispatcher*>(&packetDispatcher);
        }

        IPingPacketWriter* GetPingPacketWriter() {
            return &writer;
        }

        IMessageQueue* GetMessageQueue() {
            return static_cast<IMessageQueue*>(&recvMQ);
        }

        void StopProducers() {
            zoneEventRouter.Stop();
            broadcastPool.Stop();
            chat.Stop();
            perfCollector.Stop();
        }
        void FinishShutdown() {
            nonZoneThreadPool.Stop();
            recvMQ.Stop();
            stateManager.CleanUp();
        }
    };
}
