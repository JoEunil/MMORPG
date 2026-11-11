#pragma once

#include <memory>

#include "InMemoryQueue.h"
#include "MessagePool.h"
#include "MessageQueueHandler.h"
#include "ZoneState.h"
#include "PacketWriter.h"
#include "INetworkSender.h"
#include "IMessageQueue.h"
#include "StateManager.h"
#include "BroadcastThreadPool.h"
#include "ZoneHandler.h"
#include "ZoneThreadSet.h"
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
        InMemoryQueue recvMQ;
        MessageQueueHandler mqHandler;
        MessagePool msgPool;
        PacketWriter writer;
        StateManager stateManager;
        BroadcastThreadPool broadcastPool;
        ZoneHandler zoneHandler;
        ZoneThreadSet zoneThreadSet;
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
            zoneThreadSet.Initialize(&zoneHandler, &perfCollector);
            writer.Initialize(packetPoolManager);
            mqHandler.Initialize(networkSender, &writer, &lobbyZone, &msgPool);
            recvMQ.Initialize(&mqHandler, &msgPool);
            recvMQ.Start();
            chat.Initialize(networkSender, &writer, &perfCollector);
        }
        
        void InjectDependencies2(INetworkSender* networkSender, ISessionAuth* session, IMessageQueue* sendMQ, IProfileCache* profileCache, IInventoryOperations* inventory = nullptr) {
            stateManager.inventory = inventory;
            stateManager.Initialize(sendMQ, networkSender, &lobbyZone, &chat);
            mqHandler.InitializeStateManager(&stateManager);
            packetDispatcher.Initialize(&nonZoneThreadPool, &zoneThreadSet, &stateManager);
            nonZoneThreadPool.Start();
            broadcastPool.Start();
            ZoneState::Initialize(&broadcastPool, &writer, &stateManager, &perfCollector);
            zoneHandler.Initialize(&stateManager, networkSender, &writer);
            nonZoneHandler.Initialize(networkSender, session, &writer, &msgPool, sendMQ, &stateManager, &lobbyZone, &chat);
            nonZoneHandler.InitializeProfileCache(profileCache);
            zoneThreadSet.Start();
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
                or !zoneThreadSet.IsReady()
                or !lobbyZone.IsReady()
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
            zoneThreadSet.Stop();
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
