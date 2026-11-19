#pragma once

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
#include "SessionManager.h"
#include "BroadcastThreadPool.h"
#include "ZoneEventRouter.h"
#include "ZoneHandler.h"
#include "NonSimulationHandler.h"
#include "NonSimulationThreadPool.h"
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
        ZoneHandler zoneHandler;
        ZoneEventRouter zoneEventRouter;
        SessionManager sessionManager;
        BroadcastThreadPool broadcastPool;
        NonSimulationHandler nonSimulationHandler;
        NonSimulationThreadPool nonSimulationThreadPool;
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
            broadcastPool.Initialize(networkSender, zoneEventRouter, &perfCollector, &writer);
            lobbyZone.Initialize(& sessionManager);
            nonSimulationThreadPool.Initialize(&nonSimulationHandler);
            writer.Initialize(packetPoolManager);
            mqHandler.Initialize(networkSender, &writer, &lobbyZone, &sessionManager);
            recvMQ.Initialize(&mqHandler, &msgPool);
            recvMQ.Start();
            chat.Initialize(networkSender, &writer, &perfCollector);
        }
        
        void InjectDependencies2(INetworkSender* networkSender, ISessionAuth* session, IMessageQueue* sendMQ, IProfileCache* profileCache, IInventoryOperations* inventory = nullptr) {
            sessionManager.inventory = inventory;
            cacheCommandSender.Initialize(sendMQ, &messageWriter, &msgPool);
            zoneEventRouter.Initialize(zoneHandler, perfCollector, nonSimulationThreadPool);
            sessionManager.Initialize(&cacheCommandSender, networkSender, &lobbyZone, &chat, zoneEventRouter, zoneEventRouter);
            packetDispatcher.Initialize(nonSimulationThreadPool, zoneEventRouter, sessionManager);
            nonSimulationThreadPool.Start();
            broadcastPool.Start();
            ZoneState::Initialize(&broadcastPool, &writer, &sessionManager, &perfCollector);
            zoneHandler.Initialize(&sessionManager, networkSender, &writer, zoneEventRouter, lobbyZone, cacheCommandSender);
            nonSimulationHandler.Initialize(networkSender, session, &writer, &cacheCommandSender, &sessionManager, &lobbyZone, &chat, profileCache);
            zoneEventRouter.Start();
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
                or !sessionManager.IsReady()
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
            nonSimulationThreadPool.Stop();
            zoneEventRouter.Stop();
            broadcastPool.Stop();
            chat.Stop();
            perfCollector.Stop();
        }
        void FinishShutdown() {
            recvMQ.Stop();
            sessionManager.CleanUp();
        }
    };
}
