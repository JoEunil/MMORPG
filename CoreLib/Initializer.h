#pragma once

#include <memory>

#include "Cache/CacheCommandSender.h"
#include "Cache/InMemoryQueue.h"
#include "Cache/MessagePool.h"
#include "Cache/MessageQueueHandler.h"
#include "Cache/MessageWriter.h"
#include "World/ZoneState.h"
#include "Packet/PacketWriter.h"
#include "Interface/INetworkSender.h"
#include "Interface/IMessageQueue.h"
#include "World/SessionManager.h"
#include "World/BroadcastThreadPool.h"
#include "World/ZoneEventRouter.h"
#include "World/ZoneHandler.h"
#include "NonSimulation/NonSimulationHandler.h"
#include "NonSimulation/NonSimulationThreadPool.h"
#include "Packet/PacketDispatcher.h"
#include "World/LobbyZone.h"
#include "Interface/IPingPacketWriter.h"
#include "NonSimulation/ChatThreadPool.h"
#include "Interface/IProfileCache.h"
#include "Interface/IInventoryOperations.h"
#include "Cache/DurableLogger.h"

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
        ChatThreadPool chat;
        SessionManager sessionManager;
        BroadcastThreadPool broadcastPool;
        NonSimulationHandler nonSimulationHandler;
        NonSimulationThreadPool nonSimulationThreadPool;
        PacketDispatcher packetDispatcher;
        LobbyZone lobbyZone;
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
            mqHandler.Initialize(networkSender, &writer, &lobbyZone, &sessionManager, zoneEventRouter);
            recvMQ.Initialize(&mqHandler, &msgPool);
            recvMQ.Start();
            chat.Initialize(networkSender, &writer, &perfCollector);
        }
        
        void InjectDependencies2(INetworkSender* networkSender, ISessionAuth* session, IMessageQueue* sendMQ, IProfileCache* profileCache, IInventoryOperations* inventory = nullptr) {
            cacheCommandSender.Initialize(sendMQ, &messageWriter, &msgPool);
            zoneEventRouter.Initialize(zoneHandler, perfCollector, nonSimulationThreadPool);
            sessionManager.Initialize(chat);
            packetDispatcher.Initialize(nonSimulationThreadPool, zoneEventRouter, sessionManager);
            nonSimulationThreadPool.Start();
            broadcastPool.Start();
            ZoneState::Initialize(&broadcastPool, &writer, &sessionManager, &perfCollector, cacheCommandSender, *networkSender);
            zoneHandler.Initialize(&sessionManager, networkSender, &writer, zoneEventRouter, lobbyZone, cacheCommandSender, zoneEventRouter, inventory);
            nonSimulationHandler.Initialize(networkSender, session, &writer, &cacheCommandSender, &sessionManager, &lobbyZone, &chat, profileCache, zoneEventRouter, zoneEventRouter);
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
                or !nonSimulationHandler.IsReady()
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
            CharacterState character;
            while (lobbyZone.TakeCharacter(character))
                cacheCommandSender.EnqueueCharacterStateUpdate(character.sessionID, character);
            for (const auto& transfer : sessionManager.Shutdown())
                cacheCommandSender.EnqueueCharacterStateUpdate(transfer->character.sessionID, transfer->character);
        }
    };
}
